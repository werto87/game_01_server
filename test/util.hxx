#ifndef F913C042_CAFF_4558_AE02_952AE3C4F17A
#define F913C042_CAFF_4558_AE02_952AE3C4F17A

#include <array>
#include <iostream>
#include <vector>

template <typename T, template <typename ELEM, typename ALLOC = std::allocator<ELEM>> class Container>
std::ostream &
operator<< (std::ostream &o, const Container<T> &container)
{
  typename Container<T>::const_iterator beg = container.begin ();
  while (beg != container.end ())
    {
      o << "\n" << *beg++; // 2
    }
  return o;
}

#endif /* F913C042_CAFF_4558_AE02_952AE3C4F17A */
