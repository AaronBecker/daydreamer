
#include "daydreamer.h"

bitboard_t rank_mask[8] = {
    RANK_1_BB, RANK_2_BB, RANK_3_BB, RANK_4_BB,
    RANK_5_BB, RANK_6_BB, RANK_7_BB, RANK_8_BB
};
bitboard_t file_mask[8] = {
    FILE_A_BB, FILE_B_BB, FILE_C_BB, FILE_D_BB,
    FILE_E_BB, FILE_F_BB, FILE_G_BB, FILE_H_BB
};
bitboard_t set_mask[64];
bitboard_t clear_mask[64];
bitboard_t outpost_mask[2][64];
bitboard_t passed_mask[2][64];

void init_bitboards(void)
{
    for (int sq=0; sq<64; ++sq) {
        set_mask[sq] = 1ull<<sq;
        clear_mask[sq] = ~set_mask[sq];
        int sq_rank = sq / 8;
        int sq_file = sq % 8;
        for (int rank = sq_rank+1; rank<8; ++rank) {
            outpost_mask[WHITE][sq] |= rank_mask[rank];
            passed_mask[WHITE][sq] |= rank_mask[rank];
        }
        for (int rank = sq_rank-1; rank>=0; --rank) {
            outpost_mask[BLACK][sq] |= rank_mask[rank];
            passed_mask[BLACK][sq] |= rank_mask[rank];
        }
        bitboard_t passer_file_mask = EMPTY_BB;
        bitboard_t outpost_file_mask = EMPTY_BB;
        for (int file = MAX(0, sq_file-1); file<=MIN(7, sq_file+1); ++file) {
            passer_file_mask |= file_mask[file];
            if (file != sq_file) outpost_file_mask |= file_mask[file];
        }
        outpost_mask[WHITE][sq] &= outpost_file_mask;
        outpost_mask[BLACK][sq] &= outpost_file_mask;
        passed_mask[WHITE][sq] &= passer_file_mask;
        passed_mask[BLACK][sq] &= passer_file_mask;
    }
}

void print_bitboard(bitboard_t bb)
{
    printf("%llx\n", bb);
    for (int rank=7; rank>=0; --rank) {
        char ch;
        for (int file=0; file<8; ++file) {
            ch = '.';
            bitboard_t bit = 1ull<<(rank*8+file);
            if (bb & bit) ch = '*';
            printf("%c ", ch);
        }
        printf("\n");
    }
    printf("\n");
}
