#pragma once
#include <vector>
#include <tuple>
#include <cstdint>
#include "../types.h"
#include "../dbdata.h"
#include "qlib.h"
#include <iostream>
#include <cstdlib>


/* Compact representation of order by expressions *
 * for sorting algorithm                          */
struct OrderRequest {
    size_t   offset;
    SqlType  type;
    bool     isAscending;
};

    
class Quicksorter
{
public:
  Quicksorter(Relation* relation, const std::size_t len, std::vector<OrderRequest>* order_requests)
      : _temp_storage(std::malloc(relation->_schema._tupSize)), _relation(relation), _len(len), _order_requests(order_requests)
  {
    _randIt = Relation::RandomAccessIterator ( relation );
  }

  ~Quicksorter()
  {
    std::free(_temp_storage);
  }

  void operator()()
  {
    sort(0, _len - 1);
  }

  void sort(const std::int64_t low_index, const std::int64_t high_index)
  {
    if (low_index < high_index)
    {
      const auto pivot = partition(low_index, high_index);

      sort(low_index, pivot - 1);
      sort(pivot + 1, high_index);
    }
  }
private:
  void *_temp_storage {nullptr};
  Relation* _relation;
  Relation::RandomAccessIterator _randIt;
  const std::size_t _len;
  std::vector<OrderRequest>* _order_requests;

  /**
   * Swaps two records at the given indices.
   * @param i
   * @param j
   */
  void swap(const std::size_t i, const std::size_t j)
  {
    if (i == j)
    {
      return;
    }

    auto *dest_i = static_cast<void*>(record(i));
    auto *dest_j = static_cast<void*>(record(j));

    std::memmove(_temp_storage, dest_i, _relation->_schema._tupSize);
    std::memmove(dest_i, dest_j, _relation->_schema._tupSize); // i <- j
    std::memmove(dest_j, _temp_storage, _relation->_schema._tupSize); // j <- i
  }

  std::size_t partition(const std::int64_t low_index, const std::int64_t high_index)
  {
    auto *pivot_element = record(high_index);
    auto i = low_index;

    for (auto j = low_index; j < high_index; ++j)
    {
      if (compare(record(j), pivot_element))
      {
        swap(std::size_t(i), std::size_t(j));
        ++i;
      }
    }
    swap(std::size_t(i), std::size_t(high_index));
    return std::size_t(i);
  }

  /**
   * Returns True, when first should be ordered BEFORE second.
   *
   * @param first
   * @param second
   * @return
   */
  bool compare(Data *first, Data *second)
  {
    for(const auto& order_item : *_order_requests)
    {
      const auto c = compare ( order_item.type, 
                               &first  [ order_item.offset ], 
                               &second [ order_item.offset ] );

      if (order_item.isAscending) // ASC
      {
        if (c < 0)
        {
          return true;
        }

        if (c > 0)
        {
          return false;
        }
      }
      else // DESC
      {
        if (c > 0)
        {
          return true;
        }

        if (c < 0)
        {
          return false;
        }
      }
    }

    return false;
  }

  std::int8_t compare(const SqlType& type, Data* left, Data* right)
  {
    switch (type.tag) {
    case SqlType::BIGINT:
      return ::compare<SqlType::BIGINT>(reinterpret_cast<void*>(left), reinterpret_cast<void*>(right));
    case SqlType::INT:
      return ::compare<SqlType::INT>(reinterpret_cast<void*>(left), reinterpret_cast<void*>(right));
    case SqlType::BOOL:
      return ::compare<SqlType::BOOL>(reinterpret_cast<void*>(left), reinterpret_cast<void*>(right));
    case SqlType::DATE:
      return ::compare<SqlType::DATE>(reinterpret_cast<void*>(left), reinterpret_cast<void*>(right));
    case SqlType::DECIMAL:
      return ::compare<SqlType::DECIMAL>(reinterpret_cast<void*>(left), reinterpret_cast<void*>(right));
    case SqlType::FLOAT:
      return ::compare<SqlType::FLOAT>(reinterpret_cast<void*>(left), reinterpret_cast<void*>(right));
    case SqlType::CHAR:
      return ::compare<SqlType::CHAR>(reinterpret_cast<void*>(left), reinterpret_cast<void*>(right));
    case SqlType::VARCHAR:
      return ::compare<SqlType::VARCHAR>(reinterpret_cast<void*>(left), reinterpret_cast<void*>(right));
    default:
      return 0;
    }
  }


  Data* record(const std::size_t index)
  {
    return _randIt.get ( index );
  }
};

void sort(struct Relation* relation, std::vector<OrderRequest>* order_requests)
{
  auto sorter = Quicksorter{relation, relation->tupleNum(), order_requests};
  sorter();
}
