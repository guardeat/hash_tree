#ifndef BYTE_HASHTREE_H
#define	BYTE_HASHTREE_H

#include "sparse_vector.h"

#include <vector>
#include <xhash>
#include <type_traits>
#include <utility>
#include <limits>
#include <algorithm>
#include <queue>

namespace Byte
{

	inline constexpr size_t _EMPTY_INDEX = std::numeric_limits<size_t>::max();

	template<typename K, typename T>
	struct hash_tree_node
	{
		using value_type = std::pair<const K, T>;
		using child_container = std::vector<size_t>;

		value_type pair;
		size_t hash_value;
		child_container childs;
		size_t parent_index{ _EMPTY_INDEX };
		size_t next_index{ _EMPTY_INDEX };
	};

	template<typename K, typename T>
	class hash_tree_iterator 
	{
	private:
		using node_type = hash_tree_node<typename std::remove_const<K>::type, T>;
		using node_ptr = std::conditional_t<std::is_const<T>::value, const node_type*, node_type*>;

	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = T;
		using difference_type = std::ptrdiff_t;
		using pointer = T*;
		using reference = value_type&;

	private:
		node_ptr _nodes;
		std::vector<size_t> _visit;
		size_t _index{ 0 };

	public:
		hash_tree_iterator(node_ptr nodes, size_t head_index, size_t index, size_t size)
			:_nodes{ nodes }, _index{index}
		{
			_visit.reserve(size);
			_visit.push_back(head_index);
		}

		T& operator*()
		{
			return _nodes[_visit[_index]].pair.second;
		}

		T* operator->()
		{
			return &_nodes->at[_visit[_index]].pair.second;
		}

		hash_tree_iterator& operator++()
		{
			for (size_t i : _nodes[_visit[_index]].childs)
			{
				_visit.push_back(i);
			}
			++_index;

			return *this;
		}

		hash_tree_iterator operator++(int)
		{
			++(*this);
			return hash_tree_iterator{ *this };
		}

		bool operator==(const hash_tree_iterator& left) const
		{
			return _index == left._index;
		}

		bool operator!=(const hash_tree_iterator& left) const
		{
			return _index != left._index;
		}
	};

	template<
		typename K, 
		typename T, 
		typename Hasher = std::hash<K>, 
		typename Keyeq = std::equal_to<K>>
	class hash_tree
	{
	private:
		inline static constexpr double MAX_LOAD{ 0.9 };
		inline static constexpr double MIN_LOAD{ 0.2 };

		using node_type = hash_tree_node<K,T>;
		using node_container = sparse_vector<node_type>;
		using node_map = std::vector<size_t>;
		using head_container = std::vector<size_t>;

	public:
		using hasher = Hasher;
		using key_type = K;
		using mapped_type = T;
		using key_equal = Keyeq;

		using value_type = std::pair<const K, T>;
		using allocator_type = typename node_container::allocator_type;
		using size_type = typename node_container::size_type;
		using difference_type = typename node_container::difference_type;
		using pointer = typename node_container::pointer;
		using const_pointer = typename node_container::const_pointer;
		using reference = value_type&;
		using const_reference = const value_type&;

		using iterator = hash_tree_iterator<K, T>;
		using const_iterator = hash_tree_iterator<K, const T>;

	private:
		node_container _nodes;
		node_map _table{ _EMPTY_INDEX, _EMPTY_INDEX };
		size_t _head_index{ _EMPTY_INDEX };
		Hasher _hasher;
		Keyeq _keyeq;

	public:
		hash_tree() = default;

		hash_tree(const hash_tree& left) = default;

		hash_tree(hash_tree&& right) noexcept = default;

		hash_tree& operator=(const hash_tree& left) = default;

		hash_tree& operator=(hash_tree&& right) noexcept = default;

		~hash_tree() = default;

		void insert(const K& key, const T& value)
		{
			insert(K{ key }, T{ value });
		}

		void insert(const K& key, T&& value)
		{
			insert(K{ key }, std::move(value));
		}

		void insert(K&& key, T&& value)
		{
			size_t _index{ _insert(std::move(key), std::move(value)) };
			if (_head_index == _EMPTY_INDEX)
			{
				_head_index = _index;
			}
			else
			{
				_nodes[_head_index].childs.push_back(_index);
				_nodes[_index].parent_index = _head_index;
			}
		}

		void insert(const K& key, const T& value, const K& parent)
		{
			insert(K{ key }, T{ value }, parent);
		}

		void insert(const K& key, T&& value, const K& parent)
		{
			insert(K{ key }, std::move(value), parent);
		}

		void insert(K&& key, T&& value, const K& parent)
		{
			size_t _index{ _insert(std::move(key), std::move(value)) };
			size_t parent_index{ index(parent) };
			_nodes[parent_index].childs.push_back(_index);
			_nodes[_index].parent_index = parent_index;
		}

		void erase(const K& key)
		{
			size_t _index{ index(key) };

			if (_index == _head_index)
			{
				clear();
				return;
			}

			if (_nodes[_index].parent_index != _EMPTY_INDEX)
			{
				remove_child(_index);
			}
			
			std::queue<size_t> visit;
			visit.push(_index);
			while (!visit.empty())
			{
				for (size_t i : _nodes[visit.front()].childs)
				{
					visit.push(i);
				}
				_nodes.erase(visit.front());
				visit.pop();
			}

			if (load_factor() < MIN_LOAD)
			{
				_nodes.shrink_to_fit();
				rehash(table_size() / 2);
			}
		}

		void set_parent(const K& key, const K& new_parent)
		{
			size_t _index{ index(key) };
			size_t parent_index{ index(new_parent) };

			_set_parent(_index, parent_index, _nodes[parent_index].childs.end());
		}

		void set_parent(const K& key, const K& new_parent, size_t _index)
		{
			size_t _index{ index(key) };
			size_t parent_index{ index(new_parent) };

			_set_parent(_index, parent_index, _nodes[parent_index].childs.begin() + _index);
		}

		T& at(const K& key)
		{
			return _nodes[index(key)].pair.second;
		}

		const T& at(const K& key) const
		{
			return _nodes[index(key)].pair.second;
		}

		T& operator[](const K& key)
		{
			size_t _index{ index(key) };

			if (_index == _EMPTY_INDEX)
			{
				insert(key, T{});
				_index = index(key);
			}

			return _nodes[_index].pair.second;
		}

		const T& operator[](const K& key) const
		{
			return at(key);
		}

		bool contains(const K& key) const
		{
			return index() != _EMPTY_INDEX;
		}

		double load_factor() const
		{
			return size() / static_cast<double>(table_size());
		}

		iterator begin()
		{
			return iterator{ _nodes.data(), _head_index, 0, size() };
		}

		iterator end()
		{
			return iterator{ _nodes.data(), _EMPTY_INDEX, size(), size() };
		}

		const_iterator begin() const
		{
			return const_iterator{ _nodes.data(), _head_index, 0, size()};
		}

		const_iterator end() const
		{
			return const_iterator{ _nodes.data(), _EMPTY_INDEX, size(), size() };
		}

		size_t size() const
		{
			return _nodes.size();
		}

		size_t table_size() const
		{
			return _table.size();
		}

		void clear()
		{
			_head_index = _EMPTY_INDEX;
			_table.clear();
			_nodes.clear();
		}

	private:
		size_t index(const K& key) const
		{
			size_t hash_value{ _hasher(key) };
			size_t _index{ _table[hash_value % table_size()] };

			if (_index == _EMPTY_INDEX)
			{
				return _EMPTY_INDEX;
			}

			const node_type* it{ &_nodes[_index] };
			while (it->next_index != _EMPTY_INDEX)
			{
				if (it->hash_value == hash_value && _keyeq(it->pair.first, key))
				{
					return _index;
				}
				_index = it->next_index;
				it = &_nodes[_index];
			}

			if (it->hash_value == hash_value && _keyeq(it->pair.first, key))
			{
				return _index;
			}

			return _EMPTY_INDEX;
		}

		size_t _insert(K&& key, T&& value)
		{
			if (load_factor() > MAX_LOAD)
			{
				rehash(table_size() * 2);
			}

			size_t hash_value{ _hasher(key) };
			size_t _index{ _nodes.emplace(std::make_pair<K, T>(std::move(key), std::move(value)), hash_value) };

			insert_map(hash_value % table_size(), _index);

			return _index;
		}

		void _set_parent(size_t _index, size_t parent_index, node_container::iterator _where)
		{
			if (_nodes[_index].parent_index != _EMPTY_INDEX)
			{
				remove_child(_index);
			}

			_nodes[parent_index].childs.insert(_where, _index);
			_nodes[_index].parent_index = parent_index;
		}

		void remove_child(size_t child_index)
		{
			typename node_type::child_container& old_childs{ _nodes[_nodes[child_index].parent_index].childs };
			old_childs.erase(std::remove(old_childs.begin(), old_childs.end(), child_index), old_childs.end());
		}

		void rehash(size_t new_size)
		{
			for (node_type& node : _nodes)
			{
				node.next_index = _EMPTY_INDEX;
			}

			_table.resize(new_size);
			_table.shrink_to_fit();
			_table.assign(new_size, _EMPTY_INDEX);

			for (auto it{ _nodes.begin() }; it != _nodes.end(); ++it)
			{
				size_t map_index{ it->hash_value % table_size() };
				insert_map(map_index, it.index());
			}
		}

		void insert_map(size_t map_index, size_t node_index)
		{
			if (_table[map_index] == _EMPTY_INDEX)
			{
				_table[map_index] = node_index;
			}
			else
			{
				node_type* it{ &_nodes[_table[map_index]] };
				while (it->next_index != _EMPTY_INDEX)
				{
					it = &_nodes[it->next_index];
				}
				it->next_index = node_index;
			}
		}
	};

}

#endif
