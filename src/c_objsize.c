
#if 0
#define DBG(x) do { x; fflush(stdout); } while(0)
#else
#define DBG(x) do{}while(0)
#endif

#define DUMP 0


#define PRF(x) bitarray##x
#include "bitarray.h"

#include <caml/mlvalues.h>
#include <caml/gc.h>
#include <caml/memory.h>
#include <caml/address_class.h>

#include "util.h"
#include <memory.h>

#define Col_white (Caml_white >> 8)
#define Col_gray  (Caml_gray >> 8)
#define Col_blue  (Caml_blue >> 8)
#define Col_black (Caml_black >> 8)

/*
 * from roots.h
 */
typedef void (*scanning_action) (value, value *);
void caml_do_roots (scanning_action);

#define COLORS_INIT_COUNT 256

unsigned char* colors = NULL;
size_t colors_bitcap = 0;
size_t colors_writeindex = 0;
size_t colors_readindex = 0;


void colors_init(void)
 {
 ASSERT(colors==NULL, "colors_init");
 colors_bitcap = COLORS_INIT_COUNT*2;
 colors = bitarray_alloc(colors_bitcap);
 colors_writeindex = 0;
 colors_readindex = 0;
 return;
 }


void colors_deinit(void)
 {
 bitarray_free(colors);
 colors = NULL;
 return;
 }


void writebit(int bit)
 {
 if (colors_writeindex == colors_bitcap)
  {
  size_t colors_new_bitcap = colors_bitcap * 2;
  unsigned char* newarr = bitarray_realloc(colors, colors_new_bitcap);
  ASSERT(newarr != NULL, "realloc");
  colors = newarr;
  colors_bitcap = colors_new_bitcap;
  };
 ASSERT(colors_writeindex < colors_bitcap, "bound on write");
 bitarray_set(colors, colors_writeindex++, bit);
 return;
 }


int readbit(void)
 {
 int res;
 ASSERT(colors_readindex < colors_writeindex, "bound on read");
 res = bitarray_get(colors, colors_readindex++);
 ASSERT(res == 0 || res == 1, "bitarray_get");
 return res;
 }


void writeint(unsigned int arg, unsigned int width)
 {
 while(width-- > 0)
  {
  writebit(arg&1);
  arg >>= 1;
  };
 ASSERT(arg == 0, "writeint");
 return;
 }


unsigned int readint(unsigned int width)
 {
 unsigned int acc = 0;
 unsigned int hibit = 1 << (width-1);
 ASSERT(width > 0, "readint width");
 while(width-- > 0)
  {
  int bit = readbit();
  acc >>= 1;
  if (bit) acc |= hibit;
  };
 return acc;
 }


int prev_color = 0;
int repeat_count = 0;

#define BITS_FOR_COUNT 5
#define BITS_FOR_ORDER 4

#define MAX_REPEAT_COUNT (1<<BITS_FOR_COUNT)
#define MAX_REPEAT_ORDER (1<<BITS_FOR_ORDER)

void rle_write_repeats(void)
 {
 while(repeat_count >= MAX_REPEAT_COUNT)
  {
  unsigned int ord = 0;
  
  while(ord < MAX_REPEAT_ORDER-1 && (1<<ord) <= repeat_count/2)
   {
   ++ord;
   };
  
  writeint(Col_blue, 2);
  writeint(1, 1);
  ASSERT((1<<ord) != 0, "write_repeats#2");
  writeint(ord, BITS_FOR_ORDER);
  repeat_count -= (1 << ord);
  };
 
 ASSERT(repeat_count < MAX_REPEAT_COUNT, "write_repeats");
 
 if (repeat_count > 0)
  {
  writeint(Col_blue, 2);
  writeint(0, 1);
  writeint(repeat_count, BITS_FOR_COUNT);
  repeat_count = 0;
  };
 
 return;
 }


void rle_write_flush(void)
 {
 if (repeat_count > 0)
  {
  rle_write_repeats();
  };
 ASSERT(repeat_count == 0, "rle_write_flush");
 return;
 }


void rle_read_flush(void)
 {
 DBG(printf("rle_read_flush: repeat_count=%i, ri=%zd, wi=%zd\n",
  repeat_count, colors_readindex, colors_writeindex)
 );
 
 ASSERT
   ( repeat_count == 0
     && colors_readindex == colors_writeindex
   , "rle_reader_flush"
   );
 return;
 }


void rle_write(int color)
 {
 if (prev_color == color)
  {
  ++repeat_count;
  }
 else
  {
  rle_write_flush();
  ASSERT(color != Col_blue, "rle_write");
  writeint(color, 2);
  prev_color = color;
  };
 }


int rle_read(void)
 {
 if (repeat_count > 0)
  {
  --repeat_count;
  return prev_color;
  }
 else
  {
  int c = readint(2);
  if (c == Col_blue)
   {
   int rk = readint(1);
   if (rk == 0)
    { repeat_count = readint(BITS_FOR_COUNT); }
   else
    { repeat_count = 1 << readint(BITS_FOR_ORDER); };
   ASSERT(repeat_count > 0, "rle_read");
   return rle_read();
   }
  else
   {
   prev_color = c;
   return c;
   };
  };
 }


void rle_init(void)
 {
 prev_color = 0;
 repeat_count = 0;
 return;
 }



void writecolor(int col)
 {
 ASSERT(col >= 0 && col <= 3 && col != Col_blue, "writecolor");
 rle_write(col);
 return;
 }


int readcolor(void)
 {
 int res = rle_read();
 ASSERT(res >= 0 && res <= 3 && res != Col_blue, "readcolor");
 return res;
 }


size_t acc_hdrs;
size_t acc_data;
size_t acc_depth;
size_t g_limit_depth;
size_t acc_tags[256];
size_t acc_size[256];


#define COND_BLOCK(q) \
   (    Is_block(q) \
     && (Is_in_heap_or_young(q)) \
   )

#define GEN_COND_NOTVISITED(v, op) \
    ( Colornum_hd(Hd_val(v)) op Col_blue )

#define ENTERING_COND_NOTVISITED(v) GEN_COND_NOTVISITED(v, != )

#define RESTORING_COND_NOTVISITED(v) GEN_COND_NOTVISITED(v, == )

#define REC_WALK(cond_notvisited, rec_call, rec_goto)                  \
   size_t i;                                                           \
   value prev_block;                                                   \
   value f;                                                            \
   prev_block = Val_unit;                                              \
                                                                       \
   for (i=0; i<sz; ++i)                                                \
    {                                                                  \
    f = Field(v,i);                                                    \
    DBG(printf("(*%p)[%zd/%zd] = %p\n", (void*)v, i, sz, (void*)f));   \
                                                                       \
    if ( COND_BLOCK(f) )                                               \
     {                                                                 \
     if (prev_block != Val_unit && cond_notvisited(prev_block))        \
      {                                                                \
      rec_call                                                         \
      };                                                               \
     prev_block = f;                                                   \
     };  /* if ( COND_BLOCK ) */                                       \
    };                                                                 \
                                                                       \
   if (prev_block != Val_unit && cond_notvisited(prev_block) )         \
    {                                                                  \
    rec_goto                                                           \
    };


void c_rec_objsize(value v, size_t depth)
 {
  int col;
  header_t hd;
  size_t sz;

  if (depth >= g_limit_depth) return;

  rec_enter:

  DBG(printf("c_rec_objsize: v=%p\n"
     , (void*)v)
  );

  sz = Wosize_val(v);

  DBG(printf("after_if: v=%p\n", (void*)v));

  acc_data += sz;
  ++acc_hdrs;
  if (depth > acc_depth) { acc_depth = depth; };

  //ASSERT(Is_block(v), "scanning_hook is_block");
  //ASSERT(Tag_val(v) < 256, "Tag_val < 256");
  //printf("value %p tag %d size %ld\n", (void*)v, Tag_val(v), Wosize_val(v));
  acc_tags[Tag_val(v)]++;
  acc_size[Tag_val(v)]+=Wosize_val(v);

  hd = Hd_val(v);
  col = Colornum_hd(hd);
  writecolor(col);

  DBG(printf("COL: w %08lx %i\n", v, col));

  Hd_val(v) = Coloredhd_hd(hd, Col_blue);

  if (Tag_val(v) < No_scan_tag)
   {
   REC_WALK
    ( ENTERING_COND_NOTVISITED
    , c_rec_objsize(prev_block, (depth+1));
    , v = prev_block;                                          \
      depth = depth + 1;                                       \
      DBG(printf("goto, depth=%zd, v=%p\n", depth, (void*)v)); \
      goto rec_enter;
    )
   }; /* (Tag_val(v) < No_scan_tag) */

 return;
 }


void restore_colors(value v, size_t depth)
 {
  int col;

  if (depth >= g_limit_depth) return;

  rec_restore:

  col = readcolor();
  DBG(printf("COL: r %08lx %i\n", v, col));
  Hd_val(v) = Coloredhd_hd(Hd_val(v), col);

  if (Tag_val(v) < No_scan_tag)
   {
   size_t sz = Wosize_val(v);

   REC_WALK
    ( RESTORING_COND_NOTVISITED
    , restore_colors(prev_block, (depth + 1));
    , v = prev_block;                                          \
      depth = depth + 1; \
      goto rec_restore;
    )

   };

 return;
 }


void objsize_mark_action(value v, value* unused)
{
 (void)unused;
 /*
 DBG(printf("young heap from %p to %p\n", caml_young_start, caml_young_end));
 DBG(printf("old heap from %p to %p\n", caml_heap_start, caml_heap_end));
 */
 DBG(printf("COL writing\n"));

 if ( COND_BLOCK(v) && ENTERING_COND_NOTVISITED(v) )
  {
  c_rec_objsize(v, 0);
  };
}

void objsize_restore_action(value v, value* unused)
{
 (void)unused;
 DBG(printf("COL reading\n"));
 if ( COND_BLOCK(v) && RESTORING_COND_NOTVISITED(v) )
  {
  restore_colors(v, 0);
  };

#if DUMP
 printf("objsize: bytes for rle data = %i\n", colors_readindex/8);
 fflush(stdout);
 
  {
  FILE* f = fopen("colors-dump", "w");
  fwrite(colors, 1, colors_readindex/8, f);
  fclose(f);
  };
#endif
}

void acc_reset(size_t limit_depth)
{
  acc_hdrs = 0;
  acc_data = 0;
  acc_depth = 0;
  g_limit_depth = limit_depth;

  memset(acc_tags, 0, sizeof(acc_tags));
  memset(acc_size, 0, sizeof(acc_size));
}

void c_objsize(size_t limit_depth, value v)
 {
   DBG(printf("c_objsize %p\n", (void*)v));

   acc_reset(limit_depth);
   colors_init();
   rle_init();
   objsize_mark_action(v, NULL);
   rle_write_flush();

   rle_init();
   objsize_restore_action(v, NULL);
   rle_read_flush();
   colors_deinit();

   DBG(printf("c_objsize done.\n"));

   return;
 }

#include <caml/alloc.h>

void c_objsize_roots(size_t limit_depth)
{
#if 0
  int i;
  size_t sum_data = 0, sum_hdrs = 0;
#endif

  acc_reset(limit_depth);
  colors_init();
  rle_init();
  caml_do_roots(objsize_mark_action);
  rle_write_flush();
  rle_init();
  caml_do_roots(objsize_restore_action);
  rle_read_flush();
  colors_deinit();

#if 0
  DBG(printf("objsize_iter_roots : %zd %zd %zd\n", acc_data, acc_hdrs, acc_depth));
  for (i = 0; i < 256; i++)
  {
    sum_hdrs += acc_tags[i];
    sum_data += acc_size[i];
    if (acc_tags[i])
      DBG(printf("Tag %d : %zd : %zd\n", i, acc_tags[i], acc_size[i]));
  }
  ASSERT(sum_data == acc_data, "sum acc_data");
  ASSERT(sum_hdrs == acc_hdrs, "sum acc_hdrs");
#endif
}

value make_caml_result()
{
  CAMLparam0();
  CAMLlocal3(res, arr, v);
  int size, i, j;

  size = 0;
  for (i = 0; i < 256; i++)
  {
    if (0 == i || acc_tags[i]) size++;
  }

#if 0
  /* correction for the values allocated in this function */
  acc_tags[0] += size + 2;
  acc_size[0] += 4 * size + 4;
  acc_hdrs += size + 2;
  acc_data += 4 * size + 4;
#endif

  arr = caml_alloc_tuple(size);
  j = 0;
  for (i = 0; i < 256; i++)
  {
    if (0 == i || acc_tags[i])
    {
      ASSERT(j < size, "output index");
      v = caml_alloc_small(3, 0);
      Field(v, 0) = Val_int(i);
      Field(v, 1) = Val_int(acc_tags[i]);
      Field(v, 2) = Val_int(acc_size[i]);
      Store_field(arr, j++, v);
    }
  }

  res = caml_alloc_small(4, 0);
  Field(res, 0) = Val_int(acc_data);
  Field(res, 1) = Val_int(acc_hdrs);
  Field(res, 2) = Val_int(acc_depth);
  Field(res, 3) = arr;

  CAMLreturn(res);
}

value ml_objsize_roots(value limit)
{
  c_objsize_roots(Long_val(limit));

  return make_caml_result();
}

value ml_objsize(value limit, value start)
 {
 CAMLparam2(limit, start);
 
 c_objsize(Long_val(limit),start);

 CAMLreturn(make_caml_result());
 }

