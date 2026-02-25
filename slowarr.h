#ifndef SLOWARR_H_
#define SLOWARR_H_

#ifndef SLOWARR_NAMESPACE
# define SLOWARR_NAMESPACE(X) SLOWARR__##X
# define SLOWARR_CXXT SlowArr
#endif

#ifndef SLOWARR_FUNC
# define SLOWARR_FUNC /**/
#endif

#ifndef SLOWARR_ASSERT_USER_ERROR
# include <assert.h>
# define SLOWARR_ASSERT_USER_ERROR(expr) assert(expr)
#endif

#ifndef SLOWARR_MEMZERO
# include <string.h>
# define SLOWARR_MEMZERO(ptr, len) memset(ptr, 0, len)
#endif

#ifndef SLOWARR_MEMMOVE
# include <string.h>
# define SLOWARR_MEMMOVE(dst, src, len) memmove(dst, src, len)
#endif

#ifdef SLOW_DEFINE_ACCESS
# ifndef SLOW_DEFINE_ACCESS__DONE
#  define T(B, T) B##__##T
#  define F(B, T, f) B##__##T##__##f
#  define SLOW_DEFINE_ACCESS__DONE
# endif
#endif

#ifndef SLOWARR_SZT
# include <stddef.h>
# define SLOWARR_SZT size_t
#endif

#ifndef SLOWARR_REALLOC
# include <stdlib.h>
# define SLOWARR_REALLOC(ptr, old, news) realloc(ptr, news)
# define SLOWARR_FREE(ptr, size) free(ptr)
#endif

#ifndef SLOWARR_GROWTH_RATE
/** returns len * 1.5 */
# define SLOWARR_GROWTH_RATE(T, len) (((len) >> 1) + (len))
#endif

#ifndef SLOWARR_CAP_FOR_FIRST_ELEM
# define SLOWARR_CAP_FOR_FIRST_ELEM(T) \
   (4) /* TODO: could do better */
#endif

/* TODO */
#define SLOWARR_MANGLE(T) SLOWARR_NAMESPACE(T)
#define SLOWARR_MANGLE_F(T, F) SLOWARR_NAMESPACE(T##__##F)

#ifdef __cplusplus
# define SLOWARR_BEGINC extern "C" {
# define SLOWARR_ENDC }
#else
# define SLOWARR_BEGINC /**/
# define SLOWARR_ENDC   /**/
#endif

#ifndef SLOWARR_ON_MALLOC_FAIL
# include <stdio.h>
# include <stdlib.h>
# define SLOWARR_ON_MALLOC_FAIL(nb)                            \
   do                                                          \
   {                                                           \
     fprintf(                                                  \
         stderr, "\nmemory allocation of %lu bytes failed!\n", \
         (unsigned long) nb);                                  \
     exit(1);                                                  \
   } while (0)
#endif

#define SLOWARR__BORROWED \
  (1 << 0) /** disable reallocation / freeing */
#define SLOWARR__ZEROIZE (1 << 1) /** for cryptography */

#ifdef __cplusplus
template <typename T>
struct SLOWARR_CXXT
{};

# define SLOWARR_CXX_HEADER(T)                      \
   template <>                                      \
   struct SLOWARR_CXXT<T>                           \
   {                                                \
     SLOWARR_MANGLE(T) arr;                         \
                                                    \
     T* begin() { return arr.data; }                \
     T* cbegin() const { return arr.data; }         \
     T* cend() const { return arr.data + arr.len; } \
     T* end() { return cend(); }                    \
     SLOWARR_SZT length() const { return arr.len; } \
   };
#else
# define SLOWARR_CXX_HEADER(T) /**/
#endif

#ifdef __GNUC__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-function"
#endif
static void SLOWARR___REQUIRE_SEMI(void) {}
#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

#define SLOWARR_REQUIRE_SEMI \
  static void SLOWARR___REQUIRE_SEMI(void)

#define SLOWARR_Header(T)                                                        \
  SLOWARR_BEGINC                                                                 \
  /** usage: T(Arr,int) myarr = {0}; */                                          \
  typedef struct                                                                 \
  {                                                                              \
    SLOWARR_SZT cap, len;                                                        \
    T* data;                                                                     \
    unsigned char attr;                                                          \
  } SLOWARR_MANGLE(T);                                                           \
  SLOWARR_ENDC                                                                   \
                                                                                 \
  SLOWARR_CXX_HEADER(T)                                                          \
  SLOWARR_BEGINC                                                                 \
                                                                                 \
  /** result can not be reallocated */                                           \
  SLOWARR_FUNC SLOWARR_MANGLE(T)                                                 \
      SLOWARR_MANGLE_F(T, borrow)(T * data, SLOWARR_SZT sz);                     \
                                                                                 \
  /** usage: G(Arr,int,unsafeClear)(&arr)                                      \
   * this is marked unsafe, because it assumes that the elements don't need    \
   * to be destroyed each */ \
  SLOWARR_FUNC void                                                              \
      SLOWARR_MANGLE_F(T, unsafeClear)(SLOWARR_MANGLE(T) * arr);                 \
                                                                                 \
  /** resize to smallest cap required to hold current elems */                   \
  SLOWARR_FUNC void                                                              \
      SLOWARR_MANGLE_F(T, shrink)(SLOWARR_MANGLE(T) * arr);                      \
                                                                                 \
  SLOWARR_FUNC void SLOWARR_MANGLE_F(T, reserveTotal)(                           \
      SLOWARR_MANGLE(T) * arr, SLOWARR_SZT num);                                 \
                                                                                 \
  SLOWARR_FUNC T*                                                                \
      SLOWARR_MANGLE_F(T, pushRef)(SLOWARR_MANGLE(T) * arr);                     \
                                                                                 \
  SLOWARR_FUNC void                                                              \
      SLOWARR_MANGLE_F(T, push)(SLOWARR_MANGLE(T) * arr, T val);                 \
                                                                                 \
  /** fails if oob */                                                            \
  SLOWARR_FUNC void SLOWARR_MANGLE_F(T, remove)(                                 \
      SLOWARR_MANGLE(T) * arr, T * out, SLOWARR_SZT i);                          \
                                                                                 \
  SLOWARR_FUNC T                                                                 \
      SLOWARR_MANGLE_F(T, pop)(SLOWARR_MANGLE(T) * arr);                         \
                                                                                 \
  SLOWARR_ENDC                                                                   \
  SLOWARR_REQUIRE_SEMI

/** call this only once in your program. call SLOWARR_Header(T) first */
#define SLOWARR_Impl(T)                                          \
  SLOWARR_BEGINC                                                 \
                                                                 \
  /** result can not be reallocated */                           \
  SLOWARR_FUNC SLOWARR_MANGLE(T)                                 \
      SLOWARR_MANGLE_F(T, borrow)(T * data, SLOWARR_SZT sz)      \
  {                                                              \
    SLOWARR_MANGLE(T) arr;                                       \
    arr.data = data;                                             \
    arr.cap = sz;                                                \
    arr.len = sz;                                                \
    arr.attr = SLOWARR__BORROWED;                                \
    return arr;                                                  \
  }                                                              \
                                                                 \
  SLOWARR_FUNC void                                              \
  SLOWARR_MANGLE_F(T, unsafeClear)(SLOWARR_MANGLE(T) * arr)      \
  {                                                              \
    if (arr->data)                                               \
      SLOWARR_FREE(arr->data, arr->cap);                         \
    arr->data = (T*) (void*) 0;                                  \
    arr->cap = 0;                                                \
    arr->len = 0;                                                \
    /* don't change attrs */                                     \
  }                                                              \
                                                                 \
  /* TODO: could make this align the cap num for better perf */  \
  SLOWARR_FUNC void                                              \
  SLOWARR_MANGLE_F(T, shrink)(SLOWARR_MANGLE(T) * arr)           \
  {                                                              \
    if (arr->attr & SLOWARR__BORROWED)                           \
      return;                                                    \
                                                                 \
    if (arr->len == arr->cap)                                    \
      return;                                                    \
    if (arr->attr & SLOWARR__ZEROIZE)                            \
      SLOWARR_MEMZERO(                                           \
          arr->data + arr->len,                                  \
          (arr->cap - arr->len) * sizeof(T));                    \
    arr->data = (T*) SLOWARR_REALLOC(                            \
        arr->data, arr->cap * sizeof(T), arr->len * sizeof(T));  \
    arr->cap = arr->len;                                         \
  }                                                              \
                                                                 \
  SLOWARR_FUNC void SLOWARR_MANGLE_F(T, reserveTotal)(           \
      SLOWARR_MANGLE(T) * arr, SLOWARR_SZT num)                  \
  {                                                              \
    void* n;                                                     \
    if (num <= arr->cap)                                         \
      return;                                                    \
    SLOWARR_ASSERT_USER_ERROR(!(arr->attr & SLOWARR__BORROWED)); \
    n = SLOWARR_REALLOC(                                         \
        arr->data, sizeof(T) * arr->cap, sizeof(T) * num);       \
    if (!n)                                                      \
    {                                                            \
      if (arr->attr & SLOWARR__ZEROIZE)                          \
        SLOWARR_MEMZERO(arr->data, arr->cap * sizeof(T));        \
      SLOWARR_FREE(arr->data, arr->cap * sizeof(T));             \
      SLOWARR_ON_MALLOC_FAIL(sizeof(T) * num);                   \
    }                                                            \
    arr->data = (T*) n;                                          \
    arr->cap = num;                                              \
  }                                                              \
                                                                 \
  SLOWARR_FUNC T*                                                \
  SLOWARR_MANGLE_F(T, pushRef)(SLOWARR_MANGLE(T) * arr)          \
  {                                                              \
    if (arr->cap == 0)                                           \
    {                                                            \
      SLOWARR_MANGLE_F(T, reserveTotal)(                         \
          arr, SLOWARR_CAP_FOR_FIRST_ELEM(T));                   \
    }                                                            \
    else if (arr->len + 1 > arr->cap)                            \
    {                                                            \
      SLOWARR_MANGLE_F(T, reserveTotal)                          \
      (arr, SLOWARR_GROWTH_RATE(T, arr->len + 1));               \
    }                                                            \
    return &arr->data[arr->len++];                               \
  }                                                              \
                                                                 \
  SLOWARR_FUNC void                                              \
  SLOWARR_MANGLE_F(T, push)(SLOWARR_MANGLE(T) * arr, T val)      \
  {                                                              \
    *SLOWARR_MANGLE_F(T, pushRef)(arr) = val;                    \
  }                                                              \
                                                                 \
  SLOWARR_FUNC void SLOWARR_MANGLE_F(T, remove)(                 \
      SLOWARR_MANGLE(T) * arr, T * out, SLOWARR_SZT i)           \
  {                                                              \
    SLOWARR_SZT too_much;                                        \
    SLOWARR_ASSERT_USER_ERROR(i < arr->len);                     \
    *out = arr->data[i];                                         \
    SLOWARR_MEMMOVE(                                             \
        &arr->data[i], &arr->data[i + 1],                        \
        sizeof(T) * (arr->len - (i + 1)));                       \
    arr->len -= 1;                                               \
    too_much = arr->cap - arr->len;                              \
    if (too_much > SLOWARR_GROWTH_RATE(T, arr->len))             \
    {                                                            \
      SLOWARR_MANGLE_F(T, shrink)(arr);                          \
    }                                                            \
  }                                                              \
                                                                 \
  SLOWARR_FUNC T                                                 \
  SLOWARR_MANGLE_F(T, pop)(SLOWARR_MANGLE(T) * arr)              \
  {                                                              \
    T temp;                                                      \
    SLOWARR_MANGLE_F(T, remove)(arr, &temp, arr->len - 1);       \
    return temp;                                                 \
  }                                                              \
                                                                 \
  SLOWARR_ENDC                                                   \
  SLOWARR_REQUIRE_SEMI

#endif
