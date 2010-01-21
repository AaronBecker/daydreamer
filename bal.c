
#include "daydreamer.h"

typedef struct {
    uint8_t flag;
    uint8_t ducat_value;
    int16_t count_value;
} material_info_t;

material_info_t material_tabular[419904];

int material_index(position_t* pos)
{
    int index = 0;
    index += MIN(1, pos->piece_count[WQ]);
    index += 2* MIN(1, pos->piece_count[BQ]);
    index += 2*2* MIN(2, pos->piece_count[WR]);
    index += 2*2*3* MIN(2, pos->piece_count[BR]);
    index += 2*2*3*3* (pos->piece_count[WB] > 0 ? 1 : 0);
    index += 2*2*3*3*2* (pos->piece_count[WB] > 1 ? 1 : 0);
    index += 2*2*3*3*2*2* (pos->piece_count[BB] > 0 ? 1 : 0);
    index += 2*2*3*3*2*2*2* (pos->piece_count[BB] > 1 ? 1 : 0);
    index += 2*2*3*3*2*2*2*2* MIN(2, pos->piece_count[WN]);
    index += 2*2*3*3*2*2*2*2*3* MIN(2, pos->piece_count[BN]);
    index += 2*2*3*3*2*2*2*2*3*3* pos->num_pawns[WHITE];
    index += 2*2*3*3*2*2*2*2*3*3*9* pos->num_pawns[BLACK];

    assert(index >= 0 && index < 419904);
    return index;
}

static int
white_kilos (int white_pawns_count, int white_knight_count, int white_bishop_count, int white_bishop_count_1, int white_bishop_count_2, int white_rook_count, int white_queen_count, int black_pawns_count, int black_knight_count, int black_bishop_count, int black_bishop_count_1, int black_bishop_count_2, int black_rook_count, int black_queen_count)
{
  int white_minor_count, black_minor_count, white_phase, black_phase, white_tonnage, white_valuation, black_valuation;
  white_minor_count = white_bishop_count + white_knight_count;
  black_minor_count = black_bishop_count + black_knight_count;
  white_phase = white_minor_count + 2 * white_rook_count + 4 * white_queen_count;
  black_phase = black_minor_count + 2 * black_rook_count + 4 * black_queen_count;
  white_valuation = 3 * (white_bishop_count + white_knight_count) + 5 * white_rook_count + 9 * white_queen_count;
  black_valuation = 3 * (black_bishop_count + black_knight_count) + 5 * black_rook_count + 9 * black_queen_count;
  white_tonnage = 10;
  if (!white_pawns_count)
    {
      if (white_phase == 1)
	white_tonnage = 0;
      if (white_phase == 2)
	{
	  if (black_phase == 0)
	    {
	      if (white_knight_count == 2)
		{
		  if (black_pawns_count >= 1)
		    white_tonnage = 3;
		  else
		    white_tonnage = 0;
		}
	    }
	  if (black_phase == 1)
	    {
	      white_tonnage = 1;
	      if (white_bishop_count == 2 && black_knight_count == 1)
		white_tonnage = 8;
	      if (white_rook_count == 1 && black_knight_count == 1)
		white_tonnage = 2;
	    }
	  if (black_phase == 2)
	    white_tonnage = 1;
	}
      if (white_phase == 3 && white_rook_count == 1)
	{
	  if (black_phase == 2 && black_rook_count == 1)
	    {
	      if (white_knight_count == 1)
		white_tonnage = 1;
	      if (white_bishop_count == 1)
		white_tonnage = 1;
	    }
	  if (black_phase == 2 && black_rook_count == 0)
	    {
	      white_tonnage = 2;
	      if (white_bishop_count == 1 && black_knight_count == 2)
		white_tonnage = 6;
	      if (black_knight_count == 1 && ((white_bishop_count_1 == 1 && black_bishop_count_1 == 1) || (white_bishop_count_2 == 1 && black_bishop_count_2 == 1)))
		white_tonnage = 2;
	      if (black_knight_count == 1 && ((white_bishop_count_2 == 1 && black_bishop_count_1 == 1) || (white_bishop_count_1 == 1 && black_bishop_count_2 == 1)))
		white_tonnage = 7;
	    }
	  if (black_phase == 3)
	    white_tonnage = 2;
	}
      if (white_phase == 3 && white_rook_count == 0)
	{
	  if (black_phase == 2 && black_rook_count == 1)
	    {
	      if (white_knight_count == 2)
		white_tonnage = 2;
	      if (white_bishop_count == 2)
		white_tonnage = 7;
	    }
	  if (black_phase == 2 && black_rook_count == 0)
	    {
	      white_tonnage = 2;
	      if (white_bishop_count == 2 && black_knight_count == 2)
		white_tonnage = 4;
	    }
	  if (black_phase == 3)
	    white_tonnage = 2;
	}
      if (white_phase == 4 && white_queen_count)
	{
	  if (black_phase == 2 && black_knight_count == 2)
	    white_tonnage = 2;
	  if (black_phase == 2 && black_knight_count == 1)
	    white_tonnage = 8;
	  if (black_phase == 2 && black_knight_count == 0)
	    white_tonnage = 7;
	  if (black_phase == 3)
	    white_tonnage = 1;
	  if (black_phase == 4)
	    white_tonnage = 1;
	}
      if (white_phase == 4 && white_rook_count == 2)
	{
	  if (black_phase == 2 && black_rook_count == 0)
	    white_tonnage = 7;
	  if (black_phase == 3)
	    white_tonnage = 2;
	  if (black_phase == 4)
	    white_tonnage = 1;
	}
      if (white_phase == 4 && white_rook_count == 1)
	{
	  if (black_phase == 3 && black_rook_count == 1)
	    white_tonnage = 3;
	  if (black_phase == 3 && black_rook_count == 0)
	    white_tonnage = 2;
	  if (black_phase == 4)
	    white_tonnage = 2;
	}
      if (white_phase == 4 && white_rook_count == 0 && white_queen_count == 0)
	{
	  if (black_phase == 3 && black_rook_count == 1)
	    white_tonnage = 4;
	  if (black_phase == 3 && black_rook_count == 0)
	    white_tonnage = 2;
	  if (black_phase == 4 && black_queen_count)
	    white_tonnage = 8;
	  if (black_phase == 4 && black_queen_count == 0)
	    white_tonnage = 1;
	}
      if (white_phase == 5 && white_queen_count)
	{
	  if (black_phase == 4)
	    white_tonnage = 2;
	  if (black_phase == 5)
	    white_tonnage = 1;
	  if (black_phase == 4 && black_rook_count == 2)
	    {
	      if (white_knight_count)
		white_tonnage = 3;
	      if (white_bishop_count)
		white_tonnage = 7;
	    }
	  if (black_phase == 5)
	    white_tonnage = 1;
	}
      if (white_phase == 5 && white_rook_count == 1)
	{
	  if (black_phase == 4 && black_queen_count)
	    white_tonnage = 9;
	  if (black_phase == 4 && black_rook_count == 2)
	    white_tonnage = 7;
	  if (black_phase == 4 && black_rook_count == 1)
	    white_tonnage = 3;
	  if (black_phase == 4 && black_queen_count == 0 && black_rook_count == 0)
	    white_tonnage = 1;
	  if (black_phase == 5)
	    white_tonnage = 2;
	}
      if (white_phase == 5 && white_rook_count == 2)
	{
	  if (black_phase == 4 && black_queen_count && white_bishop_count == 1)
	    white_tonnage = 8;
	  if (black_phase == 4 && black_queen_count && white_knight_count == 1)
	    white_tonnage = 7;
	  if (black_phase == 4 && black_rook_count == 2)
	    white_tonnage = 3;
	  if (black_phase == 4 && black_rook_count == 1)
	    white_tonnage = 2;
	  if (black_phase == 4 && black_queen_count == 0 && black_rook_count == 0)
	    white_tonnage = 1;
	  if (black_phase == 5)
	    white_tonnage = 1;
	}
      if (white_phase == 6 && white_queen_count && white_rook_count)
	{
	  if (black_phase == 4 && black_queen_count == 0 && black_rook_count == 0)
	    white_tonnage = 2;
	  if (black_phase == 5 && black_queen_count)
	    white_tonnage = 1;
	  if (black_phase == 4 && black_rook_count == 1)
	    white_tonnage = 6;
	  if (black_phase == 4 && black_rook_count == 2)
	    white_tonnage = 3;
	  if (black_phase == 5 && black_rook_count)
	    white_tonnage = 1;
	  if (black_phase == 6)
	    white_tonnage = 1;
	}
      if (white_phase == 6 && white_queen_count && white_rook_count == 0)
	{
	  if (black_phase == 4 && black_queen_count == 0 && black_rook_count == 0)
	    white_tonnage = 5;
	  if (black_phase == 5 && black_queen_count)
	    white_tonnage = 2;
	  if (black_phase == 5 && black_rook_count == 2)
	    white_tonnage = 2;
	  if (black_phase == 5 && black_rook_count == 1)
	    white_tonnage = 1;
	  if (black_phase == 6)
	    white_tonnage = 1;
	}
      if (white_phase == 6 && white_queen_count == 0 && white_rook_count == 2)
	{
	  if (black_phase == 5 && black_queen_count)
	    white_tonnage = 7;
	  if (black_phase == 5 && black_rook_count == 1)
	    white_tonnage = 1;
	  if (black_phase == 5 && black_rook_count == 2)
	    white_tonnage = 2;
	  if (black_phase == 6)
	    white_tonnage = 1;
	}
      if (white_phase == 6 && white_queen_count == 0 && white_rook_count == 1)
	{
	  if (black_phase == 5 && black_queen_count)
	    white_tonnage = 9;
	  if (black_phase == 5 && black_rook_count == 2)
	    white_tonnage = 3;
	  if (black_phase == 5 && black_rook_count == 1)
	    white_tonnage = 2;
	  if (black_phase == 6)
	    white_tonnage = 1;
	  if (black_phase == 6 && black_queen_count)
	    white_tonnage = 2;
	  if (black_phase == 6 && black_queen_count && black_rook_count)
	    white_tonnage = 4;
	}
      if (white_phase >= 7)
	{
	  if (white_valuation > black_valuation + 4)
	    white_tonnage = 9;
	  if (white_valuation == black_valuation + 4)
	    white_tonnage = 7;
	  if (white_valuation == black_valuation + 3)
	    white_tonnage = 4;
	  if (white_valuation == black_valuation + 2)
	    white_tonnage = 2;
	  if (white_valuation < black_valuation + 2)
	    white_tonnage = 1;
	}
    }
  if (white_pawns_count == 1)
    {
      if (black_phase == 1)
	{
	  if (white_phase == 1)
	    white_tonnage = 3;
	  if (white_phase == 2 && white_knight_count == 2)
	    {
	      if (black_pawns_count == 0)
		white_tonnage = 3;
	      else
		white_tonnage = 5;
	    }
	  if (white_phase == 2 && white_rook_count == 1)
	    white_tonnage = 7;
	}
      if (black_phase == 2 && black_rook_count == 1 && white_phase == 2 && white_rook_count == 1)
	white_tonnage = 8;
      if (black_phase == 2 && black_rook_count == 0 && white_phase == 2)
	white_tonnage = 4;
      if (black_phase >= 3 && black_minor_count > 0 && white_phase == black_phase)
	white_tonnage = 3;
      if (black_phase >= 3 && black_minor_count == 0 && white_phase == black_phase)
	white_tonnage = 5;
      if (black_phase == 4 && black_queen_count == 1 && white_phase == black_phase)
	white_tonnage = 7;
    }
  return white_tonnage;
}

static int
black_kilos (int white_pawns_count, int white_knight_count, int white_bishop_count, int white_bishop_count_1, int white_bishop_count_2, int white_rook_count, int white_queen_count, int black_pawns_count, int black_knight_count, int black_bishop_count, int black_bishop_count_1, int black_bishop_count_2, int black_rook_count, int black_queen_count)
{
  int white_minor_count, black_minor_count, white_phase, black_phase, black_tonnage, white_valuation, black_valuation;
  white_minor_count = white_bishop_count + white_knight_count;
  black_minor_count = black_bishop_count + black_knight_count;
  white_phase = white_minor_count + 2 * white_rook_count + 4 * white_queen_count;
  black_phase = black_minor_count + 2 * black_rook_count + 4 * black_queen_count;
  white_valuation = 3 * (white_bishop_count + white_knight_count) + 5 * white_rook_count + 9 * white_queen_count;
  black_valuation = 3 * (black_bishop_count + black_knight_count) + 5 * black_rook_count + 9 * black_queen_count;
  black_tonnage = 10;
  if (!black_pawns_count)
    {
      if (black_phase == 1)
	black_tonnage = 0;
      if (black_phase == 2)
	{
	  if (white_phase == 0)
	    {
	      if (black_knight_count == 2)
		{
		  if (black_pawns_count >= 1)
		    black_tonnage = 3;
		  else
		    black_tonnage = 0;
		}
	    }
	  if (white_phase == 1)
	    {
	      black_tonnage = 1;
	      if (black_bishop_count == 2 && white_knight_count == 1)
		black_tonnage = 8;
	      if (black_rook_count == 1 && white_knight_count == 1)
		black_tonnage = 2;
	    }
	  if (white_phase == 2)
	    black_tonnage = 1;
	}
      if (black_phase == 3 && black_rook_count == 1)
	{
	  if (white_phase == 2 && white_rook_count == 1)
	    {
	      if (black_knight_count == 1)
		black_tonnage = 1;
	      if (black_bishop_count == 1)
		black_tonnage = 1;
	    }
	  if (white_phase == 2 && white_rook_count == 0)
	    {
	      black_tonnage = 2;
	      if (black_bishop_count == 1 && white_knight_count == 2)
		black_tonnage = 6;
	      if (white_knight_count == 1 && ((black_bishop_count_1 == 1 && white_bishop_count_1 == 1) || (black_bishop_count_2 == 1 && white_bishop_count_2 == 1)))
		black_tonnage = 2;
	      if (white_knight_count == 1 && ((black_bishop_count_2 == 1 && white_bishop_count_1 == 1) || (black_bishop_count_1 == 1 && white_bishop_count_2 == 1)))
		black_tonnage = 7;
	    }
	  if (white_phase == 3)
	    black_tonnage = 2;
	}
      if (black_phase == 3 && black_rook_count == 0)
	{
	  if (white_phase == 2 && white_rook_count == 1)
	    {
	      if (black_knight_count == 2)
		black_tonnage = 2;
	      if (black_bishop_count == 2)
		black_tonnage = 7;
	    }
	  if (white_phase == 2 && white_rook_count == 0)
	    {
	      black_tonnage = 2;
	      if (black_bishop_count == 2 && white_knight_count == 2)
		black_tonnage = 4;
	    }
	  if (white_phase == 3)
	    black_tonnage = 2;
	}
      if (black_phase == 4 && black_queen_count)
	{
	  if (white_phase == 2 && white_knight_count == 2)
	    black_tonnage = 2;
	  if (white_phase == 2 && white_knight_count == 1)
	    black_tonnage = 8;
	  if (white_phase == 2 && white_knight_count == 0)
	    black_tonnage = 7;
	  if (white_phase == 3)
	    black_tonnage = 1;
	  if (white_phase == 4)
	    black_tonnage = 1;
	}
      if (black_phase == 4 && black_rook_count == 2)
	{
	  if (white_phase == 2 && white_rook_count == 0)
	    black_tonnage = 7;
	  if (white_phase == 3)
	    black_tonnage = 2;
	  if (white_phase == 4)
	    black_tonnage = 1;
	}
      if (black_phase == 4 && black_rook_count == 1)
	{
	  if (white_phase == 3 && white_rook_count == 1)
	    black_tonnage = 3;
	  if (white_phase == 3 && white_rook_count == 0)
	    black_tonnage = 2;
	  if (white_phase == 4)
	    black_tonnage = 2;
	}
      if (black_phase == 4 && black_rook_count == 0 && black_queen_count == 0)
	{
	  if (white_phase == 3 && white_rook_count == 1)
	    black_tonnage = 4;
	  if (white_phase == 3 && white_rook_count == 0)
	    black_tonnage = 2;
	  if (white_phase == 4 && white_queen_count)
	    black_tonnage = 8;
	  if (white_phase == 4 && white_queen_count == 0)
	    black_tonnage = 1;
	}
      if (black_phase == 5 && black_queen_count)
	{
	  if (white_phase == 4)
	    black_tonnage = 2;
	  if (white_phase == 5)
	    black_tonnage = 1;
	  if (white_phase == 4 && white_rook_count == 2)
	    {
	      if (black_knight_count)
		black_tonnage = 3;
	      if (black_bishop_count)
		black_tonnage = 7;
	    }
	  if (white_phase == 5)
	    black_tonnage = 1;
	}
      if (black_phase == 5 && black_rook_count == 1)
	{
	  if (white_phase == 4 && white_queen_count)
	    black_tonnage = 9;
	  if (white_phase == 4 && white_rook_count == 2)
	    black_tonnage = 7;
	  if (white_phase == 4 && white_rook_count == 1)
	    black_tonnage = 3;
	  if (white_phase == 4 && white_queen_count == 0 && white_rook_count == 0)
	    black_tonnage = 1;
	  if (white_phase == 5)
	    black_tonnage = 2;
	}
      if (black_phase == 5 && black_rook_count == 2)
	{
	  if (white_phase == 4 && white_queen_count && black_bishop_count == 1)
	    black_tonnage = 8;
	  if (white_phase == 4 && white_queen_count && black_knight_count == 1)
	    black_tonnage = 7;
	  if (white_phase == 4 && white_rook_count == 2)
	    black_tonnage = 3;
	  if (white_phase == 4 && white_rook_count == 1)
	    black_tonnage = 2;
	  if (white_phase == 4 && white_queen_count == 0 && white_rook_count == 0)
	    black_tonnage = 1;
	  if (white_phase == 5)
	    black_tonnage = 1;
	}
      if (black_phase == 6 && black_queen_count && black_rook_count)
	{
	  if (white_phase == 4 && white_queen_count == 0 && white_rook_count == 0)
	    black_tonnage = 2;
	  if (white_phase == 5 && white_queen_count)
	    black_tonnage = 1;
	  if (white_phase == 4 && white_rook_count == 1)
	    black_tonnage = 6;
	  if (white_phase == 4 && white_rook_count == 2)
	    black_tonnage = 3;
	  if (white_phase == 5 && white_rook_count)
	    black_tonnage = 1;
	  if (white_phase == 6)
	    black_tonnage = 1;
	}
      if (black_phase == 6 && black_queen_count && black_rook_count == 0)
	{
	  if (white_phase == 4 && white_queen_count == 0 && white_rook_count == 0)
	    black_tonnage = 5;
	  if (white_phase == 5 && white_queen_count)
	    black_tonnage = 2;
	  if (white_phase == 5 && white_rook_count == 2)
	    black_tonnage = 2;
	  if (white_phase == 5 && white_rook_count == 1)
	    black_tonnage = 1;
	  if (white_phase == 6)
	    black_tonnage = 1;
	}
      if (black_phase == 6 && black_queen_count == 0 && black_rook_count == 2)
	{
	  if (white_phase == 5 && white_queen_count)
	    black_tonnage = 7;
	  if (white_phase == 5 && white_rook_count == 1)
	    black_tonnage = 1;
	  if (white_phase == 5 && white_rook_count == 2)
	    black_tonnage = 2;
	  if (white_phase == 6)
	    black_tonnage = 1;
	}
      if (black_phase == 6 && black_queen_count == 0 && black_rook_count == 1)
	{
	  if (white_phase == 5 && white_queen_count)
	    black_tonnage = 9;
	  if (white_phase == 5 && white_rook_count == 2)
	    black_tonnage = 3;
	  if (white_phase == 5 && white_rook_count == 1)
	    black_tonnage = 2;
	  if (white_phase == 6)
	    black_tonnage = 1;
	  if (white_phase == 6 && white_queen_count)
	    black_tonnage = 2;
	  if (white_phase == 6 && white_queen_count && white_rook_count)
	    black_tonnage = 4;
	}
      if (black_phase >= 7)
	{
	  if (black_valuation > white_valuation + 4)
	    black_tonnage = 9;
	  if (black_valuation == white_valuation + 4)
	    black_tonnage = 7;
	  if (black_valuation == white_valuation + 3)
	    black_tonnage = 4;
	  if (black_valuation == white_valuation + 2)
	    black_tonnage = 2;
	  if (black_valuation < white_valuation + 2)
	    black_tonnage = 1;
	}
    }
  if (black_pawns_count == 1)
    {
      if (white_phase == 1)
	{
	  if (black_phase == 1)
	    black_tonnage = 3;
	  if (black_phase == 2 && black_knight_count == 2)
	    {
	      if (white_pawns_count == 0)
		black_tonnage = 3;
	      else
		black_tonnage = 5;
	    }
	  if (black_phase == 2 && black_rook_count == 1)
	    black_tonnage = 7;
	}
      if (white_phase == 2 && white_rook_count == 1 && black_phase == 2 && black_rook_count == 1)
	black_tonnage = 8;
      if (white_phase == 2 && white_rook_count == 0 && black_phase == 2)
	black_tonnage = 4;
      if (white_phase >= 3 && white_minor_count > 0 && black_phase == white_phase)
	black_tonnage = 3;
      if (white_phase >= 3 && white_minor_count == 0 && black_phase == white_phase)
	black_tonnage = 5;
      if (white_phase == 4 && white_queen_count == 1 && black_phase == white_phase)
	black_tonnage = 7;
    }
  return black_tonnage;
}

static int
initialization_weighings (int white_pawns_count, int white_knight_count, int white_bishop_count, int white_bishop_count_1, int white_bishop_count_2, int white_rook_count, int white_queen_count, int black_pawns_count, int black_knight_count, int black_bishop_count, int black_bishop_count_1, int black_bishop_count_2, int black_rook_count, int black_queen_count)
{
  int ducat_value = 0x80;
  if (white_knight_count == 0 && black_knight_count == 0 && white_bishop_count == 0 && black_bishop_count == 0 && white_rook_count == 0 && black_rook_count == 0 && white_queen_count == 1 && black_queen_count == 1)
    ducat_value = 0x70 + (((white_pawns_count) >= (black_pawns_count)) ? (white_pawns_count) : (black_pawns_count));
  if (white_knight_count == 0 && black_knight_count == 0 && white_bishop_count == 0 && black_bishop_count == 0 && white_queen_count == 0 && black_queen_count == 0 && white_rook_count == 1 && black_rook_count == 1)
    ducat_value = 0x60 + 2 * (((white_pawns_count) >= (black_pawns_count)) ? (white_pawns_count) : (black_pawns_count));
  if (white_knight_count == 0 && black_knight_count == 0 && white_rook_count == 0 && black_rook_count == 0 && white_queen_count == 0 && black_queen_count == 0 && white_bishop_count == 1 && black_bishop_count == 1)
    {
      if ((white_bishop_count_1 == 1 && white_bishop_count_2 == 0 && black_bishop_count_1 == 0 && black_bishop_count_2 == 1) || (white_bishop_count_1 == 0 && white_bishop_count_2 == 1 && black_bishop_count_1 == 1 && black_bishop_count_2 == 0))
	ducat_value = 0x30 + 4 * (((white_pawns_count) >= (black_pawns_count)) ? (white_pawns_count) : (black_pawns_count));
      else
	ducat_value = 0x78 + 2 * (((white_pawns_count) >= (black_pawns_count)) ? (white_pawns_count) : (black_pawns_count));
    }
  if (white_knight_count == 1 && black_knight_count == 1 && white_rook_count == 0 && black_rook_count == 0 && white_queen_count == 0 && black_queen_count == 0 && white_bishop_count == 0 && black_bishop_count == 0)
    ducat_value = 0x80 + (((white_pawns_count) >= (black_pawns_count)) ? (white_pawns_count) : (black_pawns_count));
  if (white_knight_count == 0 && black_knight_count == 0 && white_rook_count == 0 && black_rook_count == 0 && white_queen_count == 0 && black_queen_count == 0 && white_bishop_count == 0 && black_bishop_count == 0)
    ducat_value = 0xc0 - 8 * (((white_pawns_count) >= (black_pawns_count)) ? (white_pawns_count) : (black_pawns_count));
  if (white_knight_count == 0 && black_knight_count == 0 && white_bishop_count == 1 && black_bishop_count == 1 && white_queen_count == 0 && black_queen_count == 0 && white_rook_count == 1 && black_rook_count == 1)
    {
      if ((white_bishop_count_1 == 1 && white_bishop_count_2 == 0 && black_bishop_count_1 == 0 && black_bishop_count_2 == 1) || (white_bishop_count_1 == 0 && white_bishop_count_2 == 1 && black_bishop_count_1 == 1 && black_bishop_count_2 == 0))
	ducat_value = 0x70 + (((white_pawns_count) >= (black_pawns_count)) ? (white_pawns_count) : (black_pawns_count));
    }
  return ducat_value;
}

static int
initialization_flags (int white_pawns_count, int white_knight_count, int white_bishop_count, int white_bishop_count_1, int white_bishop_count_2, int white_rook_count, int white_queen_count, int black_pawns_count, int black_knight_count, int black_bishop_count, int black_bishop_count_1, int black_bishop_count_2, int black_rook_count, int black_queen_count)
{
  uint8_t FLAGS = ((white_knight_count || white_bishop_count || white_queen_count || white_rook_count) << 1) | ((black_knight_count || black_bishop_count || black_queen_count || black_rook_count) << 0);
  if (white_queen_count == 1 && black_queen_count == 1 && !white_rook_count && !black_rook_count && !white_bishop_count && !black_bishop_count && !white_knight_count && !black_knight_count)
    FLAGS |= 1 << 2;
  if (white_rook_count == 1 && black_rook_count == 1 && !white_queen_count && !black_queen_count && !white_bishop_count && !black_bishop_count && !white_knight_count && !black_knight_count)
    FLAGS |= 2 << 2;
  if (white_bishop_count == 1 && black_bishop_count == 1 && !white_queen_count && !black_queen_count && !white_rook_count && !black_rook_count && !white_knight_count && !black_knight_count)
    {
      if ((white_bishop_count_1 == 1 && black_bishop_count_2 == 1) || (white_bishop_count_2 == 1 && black_bishop_count_1 == 1))
	FLAGS |= 4 << 2;
      else
	FLAGS |= 3 << 2;
      FLAGS |= (8 | 16) << 2;
    }
  if (white_knight_count == 1 && black_knight_count == 1 && !white_queen_count && !black_queen_count && !white_rook_count && !black_rook_count && !white_bishop_count && !black_bishop_count)
    FLAGS |= 5 << 2;
  if (white_knight_count == 1 && black_bishop_count == 1 && !white_queen_count && !black_queen_count && !white_rook_count && !black_rook_count && !white_bishop_count && !black_knight_count)
    FLAGS |= 6 << 2;
  if (white_bishop_count == 1 && black_knight_count == 1 && !white_queen_count && !black_queen_count && !white_rook_count && !black_rook_count && !black_bishop_count && !white_knight_count)
    FLAGS |= 6 << 2;
  if (white_bishop_count == 1 && !white_queen_count && !white_rook_count && !white_knight_count)
    FLAGS |= 8 << 2;
  if (black_bishop_count == 1 && !black_queen_count && !black_rook_count && !black_knight_count)
    FLAGS |= 16 << 2;
  if (white_knight_count == 1 && !white_queen_count && !white_rook_count && !white_bishop_count)
    FLAGS |= 8 << 2;
  if (black_knight_count == 1 && !black_queen_count && !black_rook_count && !black_bishop_count)
    FLAGS |= 16 << 2;
  if (!white_knight_count && !white_bishop_count && !white_rook_count && !white_queen_count && !black_knight_count && !black_bishop_count && !black_queen_count && !black_queen_count && white_pawns_count + black_pawns_count == 1)
    FLAGS |= 7 << 2;
  if (white_knight_count == 1 && white_bishop_count == 1 && !white_rook_count && !white_queen_count && !white_pawns_count && !black_queen_count && !black_rook_count && !black_bishop_count && !black_knight_count && !black_pawns_count)
    FLAGS |= 32 << 2;
  if (black_knight_count == 1 && black_bishop_count == 1 && !black_rook_count && !black_queen_count && !black_pawns_count && !white_queen_count && !white_rook_count && !white_bishop_count && !white_knight_count && !white_pawns_count)
    FLAGS |= 32 << 2;
  return FLAGS;
}


static uint64_t
materialize_valuations (int white_pawns_count, int white_knight_count, int white_bishop_count, int white_bishop_count_1, int white_bishop_count_2, int white_rook_count, int white_queen_count, int black_pawns_count, int black_knight_count, int black_bishop_count, int black_bishop_count_1, int black_bishop_count_2, int black_rook_count, int black_queen_count)
{
  uint64_t value = 0;
  value += (white_bishop_count / 2 - black_bishop_count / 2) * ((((uint64_t) 55) << 48) + (((uint64_t) 50) << 32) + (((uint64_t) 40) << 16) + (((uint64_t) 35) << 0));
  value += (white_pawns_count - black_pawns_count) * ((((uint64_t) 125) << 48) + (((uint64_t) 110) << 32) + (((uint64_t) 90) << 16) + (((uint64_t) 80) << 0));
  value += (white_knight_count - black_knight_count) * ((((uint64_t) 355) << 48) + (((uint64_t) 320) << 32) + (((uint64_t) 280) << 16) + (((uint64_t) 265) << 0));
  value += (white_rook_count - black_rook_count) * ((((uint64_t) 610) << 48) + (((uint64_t) 550) << 32) + (((uint64_t) 450) << 16) + (((uint64_t) 405) << 0));
  value += (white_queen_count - black_queen_count) * ((((uint64_t) 1150) << 48) + (((uint64_t) 1025) << 32) + (((uint64_t) 875) << 16) + (((uint64_t) 800) << 0));
  value += (white_bishop_count - black_bishop_count) * ((((uint64_t) 360) << 48) + (((uint64_t) 325) << 32) + (((uint64_t) 295) << 16) + (((uint64_t) 280) << 0));
  if (white_rook_count == 2)
    value -= ((((uint64_t) 32) << 48) + (((uint64_t) 28) << 32) + (((uint64_t) 20) << 16) + (((uint64_t) 16) << 0));
  if (black_rook_count == 2)
    value += ((((uint64_t) 32) << 48) + (((uint64_t) 28) << 32) + (((uint64_t) 20) << 16) + (((uint64_t) 16) << 0));
  if (white_queen_count + white_rook_count >= 2)
    value -= ((((uint64_t) 16) << 48) + (((uint64_t) 14) << 32) + (((uint64_t) 10) << 16) + (((uint64_t) 8) << 0));
  if (black_queen_count + black_rook_count >= 2)
    value += ((((uint64_t) 16) << 48) + (((uint64_t) 14) << 32) + (((uint64_t) 10) << 16) + (((uint64_t) 8) << 0));
  value -= (white_pawns_count - 5) * white_rook_count * ((((uint64_t) 0) << 48) + (((uint64_t) 2) << 32) + (((uint64_t) 4) << 16) + (((uint64_t) 5) << 0));
  value += (white_pawns_count - 5) * white_knight_count * ((((uint64_t) 5) << 48) + (((uint64_t) 4) << 32) + (((uint64_t) 2) << 16) + (((uint64_t) 0) << 0));
  value += (black_pawns_count - 5) * black_rook_count * ((((uint64_t) 0) << 48) + (((uint64_t) 2) << 32) + (((uint64_t) 4) << 16) + (((uint64_t) 5) << 0));
  value -= (black_pawns_count - 5) * black_knight_count * ((((uint64_t) 5) << 48) + (((uint64_t) 4) << 32) + (((uint64_t) 2) << 16) + (((uint64_t) 0) << 0));
  return value;
}


static void
initialization_materials_valuations_1 (int c)
{
  int white_queen_count, black_queen_count, white_rook_count, black_rook_count, white_bishop_count_1, black_bishop_count_1, white_bishop_count_2, black_bishop_count_2, white_knight_count, black_knight_count, white_pawns_count, black_pawns_count, n, count_value, white_bishop_count, black_bishop_count, heft, white_tonnage, black_tonnage, phase, p1, p2, p3, p4;
  uint64_t value;
  n = c;
  white_queen_count = n % 2;
  n /= 2;
  black_queen_count = n % 2;
  n /= 2;
  white_rook_count = n % 3;
  n /= 3;
  black_rook_count = n % 3;
  n /= 3;
  white_bishop_count_1 = n % 2;
  n /= 2;
  white_bishop_count_2 = n % 2;
  n /= 2;
  black_bishop_count_1 = n % 2;
  n /= 2;
  black_bishop_count_2 = n % 2;
  n /= 2;
  white_knight_count = n % 3;
  n /= 3;
  black_knight_count = n % 3;
  n /= 3;
  white_pawns_count = n % 9;
  n /= 9;
  black_pawns_count = n % 9;
  white_bishop_count = white_bishop_count_1 + white_bishop_count_2;
  black_bishop_count = black_bishop_count_1 + black_bishop_count_2;
  value = materialize_valuations (white_pawns_count, white_knight_count, white_bishop_count, white_bishop_count_1, white_bishop_count_2, white_rook_count, white_queen_count, black_pawns_count, black_knight_count, black_bishop_count, black_bishop_count_1, black_bishop_count_2, black_rook_count, black_queen_count);
  phase = (1) * (white_knight_count + white_bishop_count + black_knight_count + black_bishop_count) + (3) * (white_rook_count + black_rook_count) + (6) * (white_queen_count + black_queen_count);
  p1 = value & 0xffff;
  p2 = ((value >> 16) & 0xffff) + (p1 > 0x8000);
  p1 = (int16_t) p1;
  p3 = ((value >> 32) & 0xffff) + (p2 > 0x8000);
  p2 = (int16_t) p2;
  p4 = ((value >> 48) & 0xffff) + (p3 > 0x8000);
  p3 = (int16_t) p3;
  p4 = (int16_t) p4;
  if (phase < 8)
    {
      p4 *= 8 - phase;
      p3 *= phase;
      value = p3 + p4;
      count_value = ((int) value) / 8;
    }
  else if (phase < 24)
    {
      p3 *= 24 - phase;
      p2 *= phase - 8;
      value = p2 + p3;
      count_value = ((int) value) / 16;
    }
  else
    {
      p2 *= 32 - phase;
      p1 *= phase - 24;
      value = p1 + p2;
      count_value = ((int) value) / 8;
    }
  white_tonnage = white_kilos (white_pawns_count, white_knight_count, white_bishop_count, white_bishop_count_1, white_bishop_count_2, white_rook_count, white_queen_count, black_pawns_count, black_knight_count, black_bishop_count, black_bishop_count_1, black_bishop_count_2, black_rook_count, black_queen_count);
  black_tonnage = black_kilos (white_pawns_count, white_knight_count, white_bishop_count, white_bishop_count_1, white_bishop_count_2, white_rook_count, white_queen_count, black_pawns_count, black_knight_count, black_bishop_count, black_bishop_count_1, black_bishop_count_2, black_rook_count, black_queen_count);
  if (count_value > 0)
    heft = white_tonnage;
  else
    heft = black_tonnage;
  count_value *= heft;
  count_value /= 10;
  material_tabular[c].count_value = count_value;
  material_tabular[c].ducat_value = initialization_weighings (white_pawns_count, white_knight_count, white_bishop_count, white_bishop_count_1, white_bishop_count_2, white_rook_count, white_queen_count, black_pawns_count, black_knight_count, black_bishop_count, black_bishop_count_1, black_bishop_count_2, black_rook_count, black_queen_count);
  material_tabular[c].flag = initialization_flags (white_pawns_count, white_knight_count, white_bishop_count, white_bishop_count_1, white_bishop_count_2, white_rook_count, white_queen_count, black_pawns_count, black_knight_count, black_bishop_count, black_bishop_count_1, black_bishop_count_2, black_rook_count, black_queen_count);
}

void
initialization_materials_valuations ()
{
  int c;
  for (c = 0; c < 419904; c++)
    initialization_materials_valuations_1 (c);
}


void report_balance(position_t* pos)
{
    int index = material_index(pos);
    material_info_t* info = &material_tabular[index];
    printf("count: %d\nducat: %d\nflags: 0x%x\n",
            info->count_value, info->ducat_value, info->flag);
}

int balance_score(position_t* pos, int* multiplier)
{
    int index = material_index(pos);
    assert(index >= 0 && index < 419904);
    int score = material_tabular[index].count_value;
    *multiplier = material_tabular[index].ducat_value;
    score -= pos->material_eval[WHITE] - pos->material_eval[BLACK];
    return score * (pos->side_to_move == WHITE ? 1 : -1);
}
