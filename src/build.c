#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../include/logger.h"
#include "../include/build.h"

typedef struct { char** v; int n; int cap; } strv_t;

static void strv_init(strv_t* a){ a->v=NULL; a->n=0; a->cap=0; }
static int  strv_push(strv_t* a, char* s){
    if(a->n==a->cap){
        int nc = a->cap? a->cap*2:8;
        char** nv = (char**)realloc(a->v, (size_t)nc*sizeof(char*));
        if(!nv) return -1;
        a->v=nv; a->cap=nc;
    }
    a->v[a->n++]=s; return 0;
}
static void strv_steal_to(strv_t* a, char*** out_v, int* out_n){
    *out_v=a->v; *out_n=a->n; a->v=NULL; a->n=a->cap=0;
}
static void strv_free(strv_t* a){
    if(!a->v) return;
    for(int i=0;i<a->n;i++) free(a->v[i]);
    free(a->v); a->v=NULL; a->n=a->cap=0;
}

// strdup safe
static char* xstrdup(const char* s){
    if(!s) return NULL;
    size_t n=strlen(s)+1;
    char* p=(char*)malloc(n);
    if(p) memcpy(p,s,n);
    return p;
}

static int is_space(int c){ return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f'||c=='\v'; }

static int tokenize_shell_like(const char* line, strv_t* out_tokens){
    strv_init(out_tokens);
    const char* p=line;
    while(*p){
        while(is_space((unsigned char)*p)) p++;
        if(!*p) break;

        int in_s=0, in_d=0;
        char buf[4096]; size_t bi=0;

        while(*p){
            char c=*p;

            if(!in_s && !in_d && is_space((unsigned char)c)) break;

            if(c=='\\'){ 
                p++;
                if(*p){
                    if(bi+1>=sizeof(buf)) goto too_long;
                    buf[bi++]=*p++;
                } else {
                    // backslash final -> ignore
                }
                continue;
            }

            if(c=='\'' && !in_d){ in_s = !in_s; p++; continue; }
            if(c=='\"' && !in_s){ in_d = !in_d; p++; continue; }

            if(bi+1>=sizeof(buf)) goto too_long;
            buf[bi++]=c; p++;
        }

        if(in_s || in_d){
            LOG_ERROR("tokenize: guillemet non fermé");
            strv_free(out_tokens);
            return -1;
        }

        buf[bi]='\0';
        if(bi==0) continue;

        char* tok = xstrdup(buf);
        if(!tok || strv_push(out_tokens, tok)!=0){
            free(tok);
            strv_free(out_tokens);
            return -1;
        }
    }
    return 0;

too_long:
    LOG_ERROR("tokenize: token trop long (>%zu)", sizeof((char[4096]){0})-1);
    strv_free(out_tokens);
    return -1;
}

static int has_suffix(const char* s, const char* suf){
    size_t ns=strlen(s), nf=strlen(suf);
    return ns>=nf && strcmp(s+ns-nf, suf)==0;
}
static int is_c_file(const char* t){
    return has_suffix(t, ".c");
}
static int looks_like_compiler(const char* t){
    // heuristique simple
    return strcmp(t,"cc")==0 || strcmp(t,"gcc")==0 || strcmp(t,"clang")==0
        || strstr(t,"gcc") || strstr(t,"clang");
}
static int is_ldflag(const char* t){
    return (strncmp(t,"-L",2)==0) || (strncmp(t,"-l",2)==0)
        || (strncmp(t,"-Wl,",4)==0) || (strcmp(t,"-pthread")==0);
}
static int is_cflag(const char* t){
    if (t[0] != '-') return 0;
    if (is_ldflag(t)) return 0;
    return 1;
}

int parse_user_cmd(const char* cmdline, build_spec_t* out){
    if(!cmdline || !out){ errno=EINVAL; return -1; }
    memset(out, 0, sizeof(*out));

    strv_t toks; 
    if(tokenize_shell_like(cmdline, &toks)!=0){
        LOG_ERROR("échec du parsing de la ligne de commande");
        return -1;
    }
    if(toks.n==0){
        LOG_ERROR("commande vide");
        strv_free(&toks);
        return -1;
    }

    strv_t cfiles; strv_init(&cfiles);
    strv_t cflags; strv_init(&cflags);
    strv_t ldflags; strv_init(&ldflags);

    int i=0;
    if(looks_like_compiler(toks.v[0])){
        out->compiler = xstrdup(toks.v[0]);
        i=1;
    } else {
        out->compiler = xstrdup("cc"); 
    }
    if(!out->compiler){ LOG_ERROR("oom"); goto oom; }

    for(; i<toks.n; ++i){
        const char* t = toks.v[i];

        if(strcmp(t,"-o")==0){
            if(i+1 < toks.n){
                LOG_DEBUG("ignore la sortie binaire utilisateur: -o %s", toks.v[i+1]);
                i++; 
                continue;
            } else {
                LOG_WARN("flag -o sans argument ignoré");
                continue;
            }
        }

        if(is_c_file(t)){
            if(strv_push(&cfiles, xstrdup(t))!=0){ LOG_ERROR("oom"); goto oom; }
            continue;
        }

        if(is_ldflag(t)){
            if(strv_push(&ldflags, xstrdup(t))!=0){ LOG_ERROR("oom"); goto oom; }
            continue;
        }

        if(is_cflag(t)){
            if((strcmp(t,"-I")==0 || strcmp(t,"-D")==0 || strcmp(t,"-L")==0) && i+1<toks.n){
                const char* val = toks.v[++i];
                char buf[4096];
                int n = snprintf(buf,sizeof buf,"%s%s", t, val);
                if(n<0 || (size_t)n>=sizeof buf){ LOG_ERROR("concat flag trop long"); goto oom; }
                if(t[1]=='I' || t[1]=='D'){
                    if(strv_push(&cflags, xstrdup(buf))!=0){ LOG_ERROR("oom"); goto oom; }
                } else { // -L
                    if(strv_push(&ldflags, xstrdup(buf))!=0){ LOG_ERROR("oom"); goto oom; }
                }
                continue;
            }
            if(strv_push(&cflags, xstrdup(t))!=0){ LOG_ERROR("oom"); goto oom; }
            continue;
        }

        LOG_WARN("argument non classé '%s' → rangé dans cflags", t);
        if(strv_push(&cflags, xstrdup(t))!=0){ LOG_ERROR("oom"); goto oom; }
    }

    if(cfiles.n==0){
        LOG_ERROR("aucun fichier .c détecté dans la commande");
        goto fail;
    }

    strv_steal_to(&cfiles, &out->cfiles, &out->cfiles_n);
    strv_steal_to(&cflags, &out->cflags, &out->cflags_n);
    strv_steal_to(&ldflags, &out->ldflags, &out->ldflags_n);

    LOG_DEBUG("compiler = %s", out->compiler);
    for(int k=0;k<out->cfiles_n;k++) LOG_DEBUG("cfile[%d] = %s", k, out->cfiles[k]);
    for(int k=0;k<out->cflags_n;k++) LOG_DEBUG("cflag[%d] = %s", k, out->cflags[k]);
    for(int k=0;k<out->ldflags_n;k++) LOG_DEBUG("ldflag[%d] = %s", k, out->ldflags[k]);

    strv_free(&toks);
    return 0;

oom:
    LOG_ERROR("mémoire insuffisante");
fail:
    strv_free(&toks);
    strv_free(&cfiles);
    strv_free(&cflags);
    strv_free(&ldflags);
    free_build_spec(out);
    return -1;
}

void free_build_spec(build_spec_t* s){
    if(!s) return;
    if(s->compiler){ free(s->compiler); s->compiler=NULL; }
    if(s->cfiles){
        for(int i=0;i<s->cfiles_n;i++) free(s->cfiles[i]);
        free(s->cfiles); s->cfiles=NULL; s->cfiles_n=0;
    }
    if(s->cflags){
        for(int i=0;i<s->cflags_n;i++) free(s->cflags[i]);
        free(s->cflags); s->cflags=NULL; s->cflags_n=0;
    }
    if(s->ldflags){
        for(int i=0;i<s->ldflags_n;i++) free(s->ldflags[i]);
        free(s->ldflags); s->ldflags=NULL; s->ldflags_n=0;
    }
}
