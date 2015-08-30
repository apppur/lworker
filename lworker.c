#include <pthread.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <string.h>
#include <stdio.h>

typedef struct worker {
    lua_State *L;
    pthread_t thread;
    pthread_cond_t cond;
    const char *channel;
    struct worker *prev, *next;
} worker;

int luaopen_lworker(lua_State *L);

static worker * WSEND = NULL;
static worker * WRECV = NULL;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static worker* _self(lua_State *L) {
    worker * p;
    lua_getfield(L, LUA_REGISTRYINDEX, "_SELF");
    p = (worker *)lua_touserdata(L, -1);
    return p;
}

/* move vlaues from a sender process to a receiver*/
static void _move(lua_State *send, lua_State *recv) {
    int n = lua_gettop(send);
    int i;
    for (i = 2; i <= n; i++) {  /*1st channel*/
        lua_pushstring(recv, lua_tostring(send, i));
    }
}

static worker* _match(const char *channel, worker **queue) {
    worker *node = *queue;
    if (node == NULL) return NULL;
    do {
        if (strcmp(channel, node->channel) == 0) {
            if (*queue == node)
                *queue = (node->next == node) ? NULL : node->next;
            node->prev->next = node->next;
            node->next->prev = node->prev;
            return node;
        }
        node = node->next;
    } while (node != *queue);

    return NULL;
}

static void _wait(lua_State *L, const char *channel, worker **queue) {
    worker *p = _self(L);

    if (*queue == NULL) {
        *queue = p;
        p->prev = p->next = p;
    } else {
        p->prev = (*queue)->prev;
        p->next = *queue;
        p->prev->next = p->next->prev = p;
    }
    p->channel = channel;

    do{
        pthread_cond_wait(&p->cond, &mutex);
    } while(p->channel);
}

static void* _thread(void *arg){
    lua_State *L = (lua_State *)arg;
    luaL_openlibs(L);
    luaL_requiref(L, "lwork", luaopen_lworker, 1);
    lua_pop(L, 1);
    if(lua_pcall(L, 0, 0, 0) != 0)
        fprintf(stderr, "thread error:%s", lua_tostring(L, -1));
    printf("END PCALL\n");

    pthread_cond_destroy(&_self(L)->cond);
    lua_close(L);

    return NULL;
}

static int lsend(lua_State *L) {
    worker *p;
    const char *channel = luaL_checkstring(L, 1);
    printf("SEND : %s\n", channel);

    pthread_mutex_lock(&mutex);
    
    p = _match(channel, &WRECV);

    if (p) {
        _move(L, p->L);
        p->channel = NULL;
        pthread_cond_signal(&p->cond);
    } else {
        _wait(L, channel, &WSEND);
    }

    pthread_mutex_unlock(&mutex);

    return 0;
}

static int lrecv(lua_State *L) {
    worker *p;
    const char *channel = luaL_checkstring(L, 1);
    printf("RECV : %s\n", channel);
    lua_settop(L, 1);

    pthread_mutex_lock(&mutex);

    p = _match(channel, &WSEND);

    if (p) {
        _move(p->L, L);
        p->channel = NULL;
        pthread_cond_signal(&p->cond);
    } else {
        _wait(L, channel, &WRECV);
    }

    pthread_mutex_unlock(&mutex);

    return lua_gettop(L) - 1;
}

static int lstart(lua_State *L) {
    pthread_t thread;
    const char *chunk = luaL_checkstring(L, 1);
    printf("%s\n", chunk);
    lua_State *L1 = luaL_newstate();

    if (L1 == NULL)
        luaL_error(L, "unable to create new lua state");

    printf("load main chunk:\n");
    if (luaL_loadstring(L1, chunk) != 0)
        luaL_error(L, "error starting thread: %s", lua_tostring(L1, -1));

    if (pthread_create(&thread, NULL, _thread, L1) != 0)
        luaL_error(L, "unable to create new thread");

    //pthread_detach(thread);
    pthread_join(thread, NULL);

    return 0;
}

static int lexit(lua_State *L) {
    pthread_exit(NULL);

    return 0;
}

static const struct luaL_Reg l[] = {
    {"start", lstart},
    {"send", lsend},
    {"recv", lrecv},
    {"exit", lexit}, 
    {NULL, NULL}
};

int luaopen_lworker(lua_State *L) {
    printf("begin load worker lib\n");
    worker *self = (worker *)lua_newuserdata(L, sizeof(worker));
    lua_setfield(L, LUA_REGISTRYINDEX, "_SELF");
    self->L = L;
    self->thread = pthread_self();
    self->channel = NULL;
    pthread_cond_init(&self->cond, NULL);
    luaL_newlib(L, l);
    return 1;
}
