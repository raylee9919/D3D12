// Copyright Seong Woo Lee. All Rights Reserved.

typedef int8_t      s8;  
typedef int16_t     s16; 
typedef int32_t     s32; 
typedef int64_t     s64; 
typedef uint8_t     u8;  
typedef uint16_t    u16; 
typedef uint32_t    u32; 
typedef uint64_t    u64; 
typedef s8          b8;
typedef s16         b16;
typedef s32         b32;
typedef float       f32; 
typedef double      f64; 

#define Global   static
#define INTERNAL static

#ifdef _MSC_VER
#  define Assert(exp) if (!(exp)) { __debugbreak(); }
#else
#  define Assert(exp) if (!(exp)) { *(volatile int*)0=0; }
#endif

#ifdef _MSC_VER
#  define ASSERT(exp) if (!(exp)) { __debugbreak(); }
#else
#  define ASSERT(exp) if (!(exp)) { *(volatile int*)0=0; }
#endif

#define CountOf(Arr) (sizeof(Arr)/sizeof(Arr[0]))
#define int_from_ptr(p) (u64)(((u8*)p) - 0)
#define ptr_from_int(i) (void*)(((u8*)0) + i)
#define OffsetOf(type, member) int_from_ptr(&((type *)0)->member)
#define MemoryCopy(dst, src, sz) memcpy(dst, src, sz)

#define check_null(p) ((p)==0)
#define set_null(p) ((p)=0)
#define check_nil(nil, p) ((p)==0 || (p)==(nil))


#define sll_push_back_nz(f, l, n, nxt, zchk, zset) \
    ( ( zchk(f) ) ? \
      ( (f)=(l)=(n), zset((n)->next) ) : \
      ( (l)->nxt = (n), (l) = (n), zset((n)->nxt) ) )
#define sll_push_back_n(f, l, n, next)      sll_push_back_nz((f), (l), (n), next, check_null, set_null)
#define sll_push_back(f, l, n)              sll_push_back_nz((f), (l), (n), next, check_null, set_null)

#define sll_push_front_nz(f, l, n, next, zchk, zset) \
    ( ( zchk(f) ) ? \
      ( (f)=(l)=(n), zset((n)->next) ) : \
      ( (n)->next = (f), (f) = (n), zset((n)->next) ) )
#define sll_push_front_n(f, l, n, next)     sll_push_front_nz((f), (l), (n), next, check_null, set_null)
#define sll_push_front(f, l, n)             sll_push_front_n((f), (l), (n), next)

#define sll_pop_front_nz(f, l, next, zset) \
    ( ( (f)==(l) ) ? \
      ( zset(f), zset(l) ) : \
      ( (f)=(f)->next ) )
#define sll_pop_front(f, l) sll_pop_front_nz(f,l,next,set_null)
