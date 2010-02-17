
#include "daydreamer.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

FILE* ctg_file = NULL;
FILE* cto_file = NULL;

typedef struct {
    uint32_t pad;
    uint32_t low;
    uint32_t high;
} page_bounds_t;

page_bounds_t page_bounds;

typedef struct {
    uint16_t num_positions;
    uint16_t pad;
    char entry_buffer[4094];
} ctg_page_t;

typedef struct {
    char buf[64];
    int buf_len;
} ctg_signature_t;

typedef struct {
    int num_moves;
    uint16_t moves[50];
    int total;
    int wins;
    int losses;
    int draws;
    int reccomendation;
} ctg_entry_t;

void append_bits_reverse(ctg_signature_t* sig,
        char bits,
        int bit_position,
        int num_bits)
{
    char* sig_byte = &sig->buf[bit_position/8];
    int offset = bit_position % 8;
    for (int i=offset; i<num_bits+offset; ++i, bits>>=1) {
        if (bits & 1) *sig_byte |= 1 << (7-(i%8));
        //printf("%d", bits&1 ? 1 : 0);
        if (i%8 == 7) *(++sig_byte) = 0;
    }
    //printf(" ");
}

void print_signature(ctg_signature_t* sig)
{
    // Print bits
    printf("\n%d byte signature", sig->buf_len);
    for (int i=0; i<sig->buf_len; ++i) {
        if (i % 8 == 0) printf("\n");
        for (int j=0; j<8; ++j) {
            printf("%d", sig->buf[i] & (1<<(7-j)) ? 1 : 0);
        }
        printf(" ");
    }

    // Just print as chars
    for (int i=0; i<sig->buf_len; ++i) {
        if (i % 8 == 0) printf("\n");
        printf("%3d ", sig->buf[i]);
    }
    printf("\n");
}

int32_t ctg_signature_to_hash(ctg_signature_t* sig)
{
    static const uint32_t hash_bits[64] = {
        0x3100d2bf, 0x3118e3de, 0x34ab1372, 0x2807a847,
        0x1633f566, 0x2143b359, 0x26d56488, 0x3b9e6f59,
        0x37755656, 0x3089ca7b, 0x18e92d85, 0x0cd0e9d8,
        0x1a9e3b54, 0x3eaa902f, 0x0d9bfaae, 0x2f32b45b,
        0x31ed6102, 0x3d3c8398, 0x146660e3, 0x0f8d4b76,
        0x02c77a5f, 0x146c8799, 0x1c47f51f, 0x249f8f36,
        0x24772043, 0x1fbc1e4d, 0x1e86b3fa, 0x37df36a6,
        0x16ed30e4, 0x02c3148e, 0x216e5929, 0x0636b34e,
        0x317f9f56, 0x15f09d70, 0x131026fb, 0x38c784b1,
        0x29ac3305, 0x2b485dc5, 0x3c049ddc, 0x35a9fbcd,
        0x31d5373b, 0x2b246799, 0x0a2923d3, 0x08a96e9d,
        0x30031a9f, 0x08f525b5, 0x33611c06, 0x2409db98,
        0x0ca4feb2, 0x1000b71e, 0x30566e32, 0x39447d31,
        0x194e3752, 0x08233a95, 0x0f38fe36, 0x29c7cd57,
        0x0f7b3a39, 0x328e8a16, 0x1e7d1388, 0x0fba78f5,
        0x274c7e7c, 0x1e8be65c, 0x2fa0b0bb, 0x1eb6c371
    };

    int32_t hash = 0;
    int16_t tmp = 0;
    for (int i=0; i<sig->buf_len; ++i) {
        int8_t byte = sig->buf[i];
        tmp += ((0x0f - (byte & 0x0f)) << 2) + 1;
        hash += hash_bits[tmp & 0x3f];
        tmp += ((0xf0 - (byte & 0xf0)) >> 2) + 1;
        hash += hash_bits[tmp & 0x3f];
    }
    return hash;
}

void position_to_ctg_signature(position_t* pos, ctg_signature_t* sig)
{
    // Note: initial byte is reserved for length and flags info
    int bit_position = 8;
    char bits, num_bits;
    bool flip_board = pos->side_to_move == BLACK;
    color_t white = flip_board ? BLACK : WHITE;
    bool mirror_board = square_file(pos->pieces[white][0]) < FILE_E && pos->castle_rights == 0;
    piece_t flip_piece[] = { 0, BP, BN, BB, BR, BQ, BK, 0, 0, WP, WN, WB, WR, WQ, WK };
    for (int file=0; file<8; ++file) {
        for (int rank=0; rank<8; ++rank) {
            square_t sq = create_square(file, rank);
            if (flip_board) sq = mirror_rank(sq);
            if (mirror_board) sq = mirror_file(sq);
            piece_t piece = flip_board ? flip_piece[pos->board[sq]] : pos->board[sq];
            switch (piece) {
                case EMPTY: bits = 0x0;
                            num_bits = 1;
                            break;
                case WP: bits = 0x3;
                         num_bits = 3;
                         break;
                case BP: bits = 0x7;
                         num_bits = 3;
                         break;
                case WN: bits = 0x9;
                         num_bits = 5;
                         break;
                case BN: bits = 0x19;
                         num_bits = 5;
                         break;
                case WB: bits = 0x5;
                         num_bits = 5;
                         break;
                case BB: bits = 0x15;
                         num_bits = 5;
                         break;
                case WR: bits = 0xD;
                         num_bits = 5;
                         break;
                case BR: bits = 0x1D;
                         num_bits = 5;
                         break;
                case WQ: bits = 0x11;
                         num_bits = 6;
                         break;
                case BQ: bits = 0x31;
                         num_bits = 6;
                         break;
                case WK: bits = 0x1;
                         num_bits = 6;
                         break;
                case BK: bits = 0x21;
                         num_bits = 6;
                         break;
                default: assert(false);
            }
            append_bits_reverse(sig, bits, bit_position, num_bits);
            bit_position += num_bits;
        }
    }

    // Encode castling and en passant rights.
    int ep = -1;
    int flag_bit_length = 0;
    if (pos->ep_square) {
        ep = square_file(pos->ep_square);
        if (mirror_board) ep = 7 - ep;
        flag_bit_length = 3;
    }
    int castle = 0;
    if (has_oo_rights(pos, white)) castle += 4;
    if (has_ooo_rights(pos, white)) castle += 8;
    if (has_oo_rights(pos, white^1)) castle += 1;
    if (has_ooo_rights(pos, white^1)) castle += 2;
    if (castle) flag_bit_length += 4;
    char flag_bits = castle;
    if (ep != -1) {
        flag_bits <<= 3;
        for (int i=0; i<3; ++i, ep>>=1) if (ep&1) flag_bits |= (1<<(2-i));
    }

    //printf("\nflag bits: %d\n", flag_bits);
    //printf("bit_position: %d\n", bit_position%8);
    //printf("flag_bit_length: %d\n", flag_bit_length);

    // Insert padding so that flags fit at the end of the last byte.
    int pad_bits = 0;
    if (8-(bit_position % 8) < flag_bit_length) {
        //printf("padding byte\n");
        pad_bits = 8 - (bit_position % 8);
        append_bits_reverse(sig, 0, bit_position, pad_bits);
        bit_position += pad_bits;
    }

    pad_bits = 8 - (bit_position % 8) - flag_bit_length;
    if (pad_bits < 0) pad_bits += 8;
    //printf("padding %d bits\n", pad_bits);
    append_bits_reverse(sig, 0, bit_position, pad_bits);
    bit_position += pad_bits;
    append_bits_reverse(sig, flag_bits, bit_position, flag_bit_length);
    bit_position += flag_bit_length;
    sig->buf_len = (bit_position + 7) / 8;

    // Write header byte
    sig->buf[0] = ((char)(sig->buf_len));
    if (ep != -1) sig->buf[0] |= 1<<5;
    if (castle) sig->buf[0] |= 1<<6;
}

void init_ctg(char* filename)
{
    int name_len = strlen(filename);
    assert(filename[name_len-3] == 'c' &&
            filename[name_len-2] == 't' &&
            filename[name_len-1] == 'g');
    char fbuf[1024];
    strcpy(fbuf, filename);
    if (ctg_file) {
        assert(cto_file);
        fclose(ctg_file);
        fclose(cto_file);
    }
    ctg_file = fopen(fbuf, "r");
    fbuf[name_len-1] = 'o';
    cto_file = fopen(fbuf, "r");
    fbuf[name_len-1] = 'b';
    FILE* ctb_file = fopen(fbuf, "r");
    fbuf[name_len-1] = 'g';
    if (!ctg_file || !cto_file || !ctb_file) {
        printf("info string Couldn't load book %s\n", fbuf);
        return;
    }

    // Read out upper and lower page limits.
    fread(&page_bounds, 12, 1, ctb_file);
    page_bounds.low = ntohl(page_bounds.low);
    page_bounds.high = ntohl(page_bounds.high);
    assert(page_bounds.low <= page_bounds.high);
    fclose(ctb_file);

    printf("low %d high %d\n", page_bounds.low, page_bounds.high);
}

bool ctg_get_page_index(int hash, int* page_index)
{
    for (uint32_t mask = 1, key = (hash & mask) + mask;
            key <= page_bounds.high; mask = (mask << 1) + 1) {
        if (key >= page_bounds.low) {
            fseek(cto_file, 16 + (key-1) * 4, SEEK_SET);
            fread(page_index, 4, 1, cto_file);
            return true;
        }
    }
    return false;
}

bool ctg_read_page(int page_index, ctg_entry_t* entry)
{
}

int main(int argc, char* argv[])
{
    char* book = "book.ctg";
    if (argc > 1) book = argv[1];
    printf("reading book %s\n", book);
    init_ctg(book);

    position_t pos;
    ctg_signature_t sig;
    char buf[1024];
    while (fgets(buf, 1024, stdin)) {
        set_position(&pos, buf);
        position_to_ctg_signature(&pos, &sig);
        print_signature(&sig);
    }
}

/*
		if (c >= lower_bound){
			//=== get page number in the cto-file ===
			//We need this for the ctg-file later on 
			pathTemp[int(strlen(bookPath))-1]='o';
			FILE* ctoFile;
			fopen_s(&ctoFile, pathTemp , "r");
			_set_fmode(_O_BINARY);
			if (ctoFile != NULL) {
				char line[1000];

				//get the number of entries in total
				fread(line , 1, 16, ctoFile);
				//int totalEntries=unsigned char((line[4])<<24)+(unsigned char(line[5])<<16)+(unsigned char(line[6])<<8)+unsigned char(line[7]);
				//printf("skip c=%d entries in cto\n", c);
				for(unsigned int i=0; i<=c; i++){
					fread(line , 1, 4, ctoFile);
				}
				int pageNumber=unsigned char((line[0])<<24)+(unsigned char(line[1])<<16)+(unsigned char(line[2])<<8)+unsigned char(line[3]);
				fclose(ctoFile);
				//Now probe ctg-file! 
				if(line[0]!=-1 || line[1]!=-1 || line[2]!=-1 || line[3]!=-1){
					if(getBookEntryFromPage(pageNumber, bb, dEntry1)){
						return 1;
					}
				}

			}
			else{
				printf("Error reading cto-file: ");
				printf(pathTemp);
				printf("\n");
			}
			if (c>=upper_bound){break;}
		}
	}


	//We found no entry in the book. // 
	return 0;
}
*/
