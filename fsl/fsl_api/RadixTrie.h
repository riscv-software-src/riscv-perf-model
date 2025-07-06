// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file RadixTrie.h  space optimized search tree(trie)
#pragma once
#include <chrono>
#include <cstdint>
#include <iostream>
#include <iostream>
#include <limits>
#include <memory>
#include <random>

//! \brief Node for uint32_t radix trie
//!
template <uint32_t BIT_WIDTH> class RadixTrieNode
{
  public:
    //! \brief ...
    bool isEndOfWord;
    //! \brief ...
    std::vector<std::unique_ptr<RadixTrieNode>> children;

    //! \brief ...
    RadixTrieNode() : isEndOfWord(false)
    {
        children.resize(1 << BIT_WIDTH);
        for (auto & child : children)
        {
            child = nullptr;
        }
    }
};

//! \brief RadixTrie with element width as template parameter
//!
//! This is self explanatory. 4 bits seems to be the most performant,
//! for state sizes of 1024*1024.
//!
//! This is not used in the current implementation. It is provided
//! in the DPR for comment on future use and because I am comparing
//! performance against real sets of fusion groups. There is little
//! to no protection against 'bad' input. That will come later.
//!
//! 1            1024*1024
//! Time taken for insertion: 7.31112 seconds
//! Time taken for searching: 1.26558 seconds
//! 2            1024*1024
//! Time taken for insertion: 4.85911 seconds
//! Time taken for searching: 0.691898 seconds
//! 4            1024*1024
//! Time taken for insertion: 4.77562 seconds      <=== 4 bits
//! Time taken for searching: 0.43249 seconds
//! 8            1024*1024
//! Time taken for insertion: 25.8369 seconds
//! Time taken for searching: 0.319623 seconds
template <uint32_t BIT_WIDTH> class RadixTrie
{
  public:
    //! \brief per convention used elsewhere
    typedef std::shared_ptr<RadixTrie> PtrType;

    //! \brief default ctor
    RadixTrie() : root(std::make_unique<RadixTrieNode<BIT_WIDTH>>()) {}

    //! \brief insert...
    void insert(uint32_t key) { insertRecursive(root, key, 0); }

    //! \brief find...
    bool search(uint32_t key) const
    {
        return searchRecursive(root, key, 0);
    }

  private:
    //! \brief ...
    void insertRecursive(std::unique_ptr<RadixTrieNode<BIT_WIDTH>> & node,
                         uint32_t key, uint32_t depth)
    {
        if (!node)
        {
            node = std::make_unique<RadixTrieNode<BIT_WIDTH>>();
        }

        if (depth == MAX_DEPTH)
        {
            node->isEndOfWord = true;
            return;
        }

        uint32_t index = (key >> (BIT_WIDTH * (MAX_DEPTH - depth - 1)))
                         & ((1 << BIT_WIDTH) - 1);
        insertRecursive(node->children[index], key, depth + 1);
    }

    //! \brief ...
    bool
    searchRecursive(const std::unique_ptr<RadixTrieNode<BIT_WIDTH>> & node,
                    uint32_t key, uint32_t depth) const
    {
        if (!node)
        {
            return false;
        }
        if (depth == MAX_DEPTH)
        {
            return node->isEndOfWord;
        }
        uint32_t index = (key >> (BIT_WIDTH * (MAX_DEPTH - depth - 1)))
                         & ((1 << BIT_WIDTH) - 1);
        return searchRecursive(node->children[index], key, depth + 1);
    }

    //! \brief top of the trie
    std::unique_ptr<RadixTrieNode<BIT_WIDTH>> root;

    //! \brief keep a limit on depth based on the template parameter
    static constexpr uint32_t MAX_DEPTH =
        std::numeric_limits<uint32_t>::digits / BIT_WIDTH;
};
