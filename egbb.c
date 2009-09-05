
#include "daydreamer.h"
#include <string.h>

#ifdef __APPLE__
#include <dlfcn.h>
#define EGBB_NAME       "egbbdylib.dylib"
#define EGBB64_NAME     "egbbdylib64.dylib"
#define load_library(x) dlopen((x), RTLD_LAZY)
#define load_function   dlsym
#define unload_library  dlclose
#define lib_handle_t    void*
#endif

#ifdef __linux__
#include <dlfcn.h>
#define EGBB_NAME       "egbbso.so"
#define EGBB64_NAME     "egbbso64.so"
#define load_library(x) dlopen((x), RTLD_LAZY)
#define load_function   dlsym
#define unload_library  dlclose
#define lib_handle_t    void*
#endif

#ifdef _WIN32
#define EGBB_NAME       "egbbdll.dll"
#define EGBB64_NAME     "egbbdll64.dll"
#define load_library    LoadLibrary
#define load_function   GetProcAddress
#define unload_library  FreeLibrary
#define lib_handle_t    HMODULE
#endif

enum egbb_colors {
    _WHITE,_BLACK
};
enum egbb_occupancy {
    _EMPTY,
    _WKING,_WQUEEN,_WROOK,_WBISHOP,_WKNIGHT,_WPAWN,
    _BKING,_BQUEEN,_BROOK,_BBISHOP,_BKNIGHT,_BPAWN
};
enum egbb_load_types {
    LOAD_NONE,LOAD_4MEN,SMART_LOAD,LOAD_5MEN
};

static const int piece_to_egbb_table[] = {
    _EMPTY, _WPAWN, _WKNIGHT, _WROOK, _WQUEEN, _WKING, _EMPTY,
    _EMPTY, _BPAWN, _BKNIGHT, _BROOK, _BQUEEN, _BKING, _EMPTY
};
#define piece_to_egbb(p)    piece_to_egbb_table[p]
#define square_to_egbb(s)   square_to_index(s)
#define EGBB_NOTFOUND 99999

typedef int (*probe_egbb_fn)(int side, int wk, int bk,
        int p1, int s1,
        int p2, int s2,
        int p3, int s3);
typedef void (*load_egbb_fn)(char* path, int cache_size, int options);


static probe_egbb_fn probe_egbb_internal;

bool load_egbb(char* egbb_dir, int cache_size_bytes)
{
    static lib_handle_t lib = NULL;
    char path[1024];
    strncpy(path, egbb_dir, 1000);
    strcat(path, EGBB_NAME);

    if (lib) unload_library(lib);
    lib = load_library(path);
    if (!lib) {
        strncpy(path, egbb_dir, 1000);
        strcat(path, EGBB64_NAME);
        lib = load_library(path);
    }
    if (!lib) return false;

    load_egbb_fn load_egbb = load_function(lib, "load_egbb_5men");
    probe_egbb_internal = load_function(lib, "probe_egbb_5men");
    load_egbb(egbb_dir, cache_size_bytes, LOAD_4MEN);
    return true;
}

bool probe_egbb(position_t* pos, int* value)
{
    assert(pos->num_pieces[WHITE] + pos->num_pieces[BLACK] +
            pos->num_pawns[WHITE] + pos->num_pawns[BLACK] <= 5);
    int wk = square_to_egbb(pos->pieces[WHITE][0]);
    int bk = square_to_egbb(pos->pieces[BLACK][0]);
    int p[3] = { _EMPTY, _EMPTY, _EMPTY };
    int s[3] = { _EMPTY, _EMPTY, _EMPTY };
    int count=0;
    square_t sq;
    for (int i=1; i<pos->num_pieces[WHITE]; ++i) {
        sq = pos->pieces[WHITE][i];
        p[count] = piece_to_egbb(pos->board[sq]);
        s[count++] = square_to_egbb(sq);
    }
    for (int i=1; i<pos->num_pieces[BLACK]; ++i) {
        sq = pos->pieces[BLACK][i];
        p[count] = piece_to_egbb(pos->board[sq]);
        s[count++] = square_to_egbb(sq);
    }
    for (int i=1; i<pos->num_pawns[WHITE]; ++i) {
        sq = pos->pawns[WHITE][i];
        p[count] = piece_to_egbb(pos->board[sq]);
        s[count++] = square_to_egbb(sq);
    }
    for (int i=1; i<pos->num_pawns[BLACK]; ++i) {
        sq = pos->pawns[BLACK][i];
        p[count] = piece_to_egbb(pos->board[sq]);
        s[count++] = square_to_egbb(sq);
    }
    assert(count <= 3);

    *value = probe_egbb_internal(pos->side_to_move,
            wk, bk, p[0], s[0], p[1], s[1], p[2], s[2]);

    return (*value != EGBB_NOTFOUND);
}

