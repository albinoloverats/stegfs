#include <sys/types.h>
#include "tiger.h"

/* The compress function is a function. Requires smaller cache?    */
void tiger_compress(uint64_t *str, uint64_t state[3])
{
  tiger_compress_macro(((uint64_t*)str), ((uint64_t*)state));
}

void tiger(uint64_t *str, uint64_t length, uint64_t res[3])
{
  register uint64_t i, j;
  uint8_t temp[64];

  res[0]=0x0123456789ABCDEFLL;
  res[1]=0xFEDCBA9876543210LL;
  res[2]=0xF096A5B4C3B2E187LL;

  for(i=length; i>=64; i-=64)
    {
#ifdef TIGER_BIG_ENDIAN
      for(j=0; j<64; j++)
    temp[j^7] = ((byte*)str)[j];
      tiger_compress(((uint64_t*)temp), res);
#else
      tiger_compress(str, res);
#endif
      str += 8;
    }

#ifdef TIGER_BIG_ENDIAN
  for(j=0; j<i; j++)
    temp[j^7] = ((byte*)str)[j];

  temp[j^7] = 0x01;
  j++;
  for(; j&7; j++)
    temp[j^7] = 0;
#else
  for(j=0; j<i; j++)
    temp[j] = ((byte*)str)[j];

  temp[j++] = 0x01;
  for(; j&7; j++)
    temp[j] = 0;
#endif
  if(j>56)
    {
      for(; j<64; j++)
    temp[j] = 0;
      tiger_compress(((uint64_t*)temp), res);
      j=0;
    }

  for(; j<56; j++)
    temp[j] = 0;
  ((uint64_t*)(&(temp[56])))[0] = ((uint64_t)length)<<3;
  tiger_compress(((uint64_t*)temp), res);
}
