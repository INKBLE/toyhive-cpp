#pragma once
#include <cstddef>
#include <new>
#include <iterator>
#include <sys/types.h>
#include <cstdint>
#include <cassert>
#include <utility>

template <typename T>
class hive
{

public:
    using size_t = std::size_t;

private:
    static constexpr size_t BLOCK_CAPACITY = 64;
    using skip_t = std::uint8_t;
    size_t size_{0};

    struct block
    {
        block* prev{nullptr};
        block* next{nullptr};
        T* data{nullptr};
        size_t active_count{0}; //记录元素数量
        skip_t first_free_idx{0}; // 记录当前块内已知的第一个可用空洞起点
        skip_t skipfield[BLOCK_CAPACITY]{0};

        explicit block(block* prev = nullptr, block* next = nullptr) : prev(prev), next(next), active_count(0)
        {
            data = static_cast<T*>(::operator new(sizeof(T) * BLOCK_CAPACITY, std::align_val_t{alignof(T)}));
            skipfield[0] = BLOCK_CAPACITY;
            skipfield[BLOCK_CAPACITY - 1] = BLOCK_CAPACITY;
            first_free_idx = 0;
        }

        ~block()
        {
            if (active_count > 0)
            {
                for (size_t i = 0; i < BLOCK_CAPACITY; ++i)
                {
                    if (skipfield[i] == 0)
                    {
                        data[i].~T();
                    }
                    else
                    {
                        i += skipfield[i] - 1;
                    }
                }
            }
            ::operator delete(data, std::align_val_t{alignof(T)});
        }


        template <typename... Args>
        T* emplace_at(size_t idx, Args&&... args)
        {
            assert(idx < BLOCK_CAPACITY && "idx不能越界");
            assert(skipfield[idx] && "idx必须是空洞的起点");

            // 在指定的空闲 idx 上执行 Placement new
            T* ptr = ::new (static_cast<void*>(&data[idx])) T(std::forward<Args>(args)...);

            // 维护 block 内部状态与 Skipfield
            ++active_count;
            if(skipfield[idx] != 1) //当前空洞还有
            {
                skipfield[idx + skipfield[idx] - 1] = skipfield[idx] - 1;
                skipfield[idx + 1] = skipfield[idx] - 1;
                if(first_free_idx == idx) { ++first_free_idx; }
            }
            else if(active_count < BLOCK_CAPACITY)
            {
                for(size_t i = idx + 1; i < BLOCK_CAPACITY; ++i)
                {
                    if(skipfield[i] != 0)
                    {
                        first_free_idx = i;
                        break;
                    }
                }
            }

            skipfield[idx] = 0;
            return ptr;
        }

    T* erase_at(size_t idx)
    {
        assert(this != nullptr && "blk 不能为空");
        assert(idx < BLOCK_CAPACITY && "idx 不能越界");
        assert(this->skipfield[idx] == 0 && "不能删除不存在的元素");

        // 显式销毁对象
        this->data[idx].~T();

        // 探查左右两侧空洞长度（0 表示邻居有数据或已到边界）
        const size_t left_len  = (idx != 0) ? this->skipfield[idx - 1] : 0;
        const size_t right_len = (idx != BLOCK_CAPACITY - 1) ? this->skipfield[idx + 1] : 0;

        // 计算合并后的完整空洞长度
        const size_t total_len = left_len + 1 + right_len;

        // 定位合并后区间的左右边界
        const size_t left_boundary  = idx - left_len;
        const size_t right_boundary = idx + right_len;

        // 仅更新两端哨兵
        this->skipfield[left_boundary]  = total_len;
        this->skipfield[right_boundary] = total_len;

        // 维护块状态
        --this->active_count;

        // 更新首个空闲槽位索引
        if (idx < this->first_free_idx)
        {
            this->first_free_idx = idx;
        }

        return &this->data[idx];
    }

    };
    block* head_block_{nullptr};
    block* tail_block_{nullptr};


public:
    hive() = default;

    hive(const hive& other)
    {
        // const_iterator 尚未作为公开 API 提供；复制只读访问源容器。
        hive& source = const_cast<hive&>(other);
        for(auto it = source.begin(); it != source.end(); ++it)
        {
            emplace(*it);
        }
    }

    hive& operator=(const hive& other)
    {
        if(this != &other)
        {
            hive temp(other);
            swap(temp);
        }
        return *this;
    }

    void swap(hive& other) noexcept
    {
        std::swap(head_block_, other.head_block_);
        std::swap(tail_block_, other.tail_block_);
        std::swap(size_, other.size_);
    }

    hive(hive&& other) noexcept :
        size_(std::exchange(other.size_, 0)),
        head_block_(std::exchange(other.head_block_, nullptr)),
        tail_block_(std::exchange(other.tail_block_, nullptr)) {}

    hive& operator=(hive&& other) noexcept
    {
        if(this != &other)
        {
            clear();
            size_ = std::exchange(other.size_, 0);
            head_block_ = std::exchange(other.head_block_, nullptr);
            tail_block_ = std::exchange(other.tail_block_, nullptr);
        }
        return *this;
    }

    ~hive()
    {
        clear();
    }

    void clear() noexcept
    {
        block* curr = head_block_;
        while(curr != nullptr)
        {
            block* next = curr->next;
            delete curr;
            curr = next;
        }
        head_block_ = tail_block_ = nullptr;
        size_ = 0;
    }

    [[nodiscard]] size_t size () const noexcept { return size_; }

    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    class iterator {
        friend class hive;

    public:

        // 标准迭代器特征类型别名
        using value_type        = T;
        using reference         = T&;
        using pointer           = T*;
        using difference_type   = std::ptrdiff_t;
        using iterator_category = std::bidirectional_iterator_tag;


        // 构造函数
        iterator() noexcept : curr_block_(nullptr), index_(0) {}
        iterator(block* b, size_t index) noexcept : curr_block_(b), index_(index) {}

        // 访问操作符
        [[nodiscard]] reference operator*() const noexcept
        {
            return curr_block_->data[index_];
        }

        [[nodiscard]] pointer operator->() const noexcept
        {
            return &curr_block_->data[index_];
        }

        // 移动操作符
        iterator& operator++() noexcept     // 前置 ++
        {
            advance_to_next();
            return *this;
        }
        iterator  operator++(int) noexcept   // 后置 ++
        {
            iterator temp = *this;
            ++(*this);
            return temp;
        }

        iterator& operator--() noexcept       // 前置 --
        {
            retreat_to_prev();
            return *this;
        }
        iterator  operator--(int) noexcept    // 后置 --
        {
            iterator temp = *this;
            --(*this);
            return temp;
        }

        // 比较操作符
        [[nodiscard]] bool operator==(const iterator& other) const noexcept
        {
            return this->curr_block_ == other.curr_block_ && this->index_ == other.index_;
        }
        [[nodiscard]] bool operator!=(const iterator& other) const noexcept
        {
            return !(this->curr_block_ == other.curr_block_ && this->index_ == other.index_);
        }

    private:
        block* curr_block_{nullptr}; // 当前指向的内存块
        size_t index_{0};            // 当前在块内的槽位下标

        void advance_to_next() noexcept // 向后扫描/跳跃到下一个存在元素的槽位
        {
            if (curr_block_ == nullptr) return;
            ++index_;
            while (curr_block_ != nullptr)
            {
                while(index_ < BLOCK_CAPACITY && curr_block_->skipfield[index_] > 0)
                {
                    index_ += curr_block_->skipfield[index_];
                }

                if (index_ < BLOCK_CAPACITY) { return; }
                if (curr_block_->next != nullptr)
                {
                    curr_block_ = curr_block_->next;
                    index_ = 0;
                }
                else
                {
                    index_ = BLOCK_CAPACITY;
                    return;
                }
            }
        }

        void retreat_to_prev() noexcept // 向前扫描/跳跃到上一个存在元素的槽位
        {
            assert(curr_block_ != nullptr && "空迭代器不能回跳");

            if (index_ == 0)
            {
                curr_block_ = curr_block_->prev;
                index_ = BLOCK_CAPACITY - 1;
            }
            else { --index_; }

            while (curr_block_ != nullptr)
            {
                while (index_ < BLOCK_CAPACITY && curr_block_->skipfield[index_] > 0)
                {
                    skip_t len = curr_block_->skipfield[index_];
                    if (index_ < len) {
                        index_ = 0;
                        break;
                    }
                    index_ -= len;
                }

                if (curr_block_->skipfield[index_] == 0) {
                    return;
                }
                curr_block_ = curr_block_->prev;
                index_ = BLOCK_CAPACITY - 1;
            }
        }
    };

public:

    [[nodiscard]] iterator begin() noexcept
    {
        if (head_block_ == nullptr) { return end(); }

        iterator it(head_block_, 0);
        if (head_block_->skipfield[0] > 0) {
            it.index_ = head_block_->skipfield[0];
            if (it.index_ >= BLOCK_CAPACITY)
            {
                it.advance_to_next();
            }
        }
        return it;
    }

    [[nodiscard]] iterator end() noexcept
    {
        if (tail_block_ == nullptr)
        {
            return iterator(nullptr, 0);
        }
        return iterator(tail_block_, BLOCK_CAPACITY);
    }

    template <typename... Args>
    iterator emplace(Args&&... args)
    {
        auto [curr, idx] = find_slot_location();
        if(curr == nullptr)
        {
            block* new_block_ = new block(tail_block_, nullptr);
            if(tail_block_ != nullptr) { tail_block_->next = new_block_; }
            else { head_block_ = new_block_; }
            tail_block_ = new_block_;
            curr = new_block_;
            idx = 0;
        }
        curr->emplace_at(idx, std::forward<Args>(args)...);
        ++size_;
        return iterator(curr, idx);
    }
    iterator insert(const T& value)
    {
        return emplace(value);
    }

    iterator insert(T&& value)
    {
        return emplace(std::move(value));
    }

    iterator erase(iterator it)
    {
        block* blk = it.curr_block_;
        size_t idx = it.index_;

        assert(blk != nullptr && idx < BLOCK_CAPACITY);
        assert(blk->skipfield[idx] == 0 && "不能删除不存在的元素");

        // 记录右侧原有的空洞长度 R
        const size_t right_len = (idx != BLOCK_CAPACITY - 1) ? blk->skipfield[idx + 1] : 0;

        // 析构并合并 skipfield
        blk->erase_at(idx);
        --size_;

        // 下一个有效位置就在当前 idx 跨过右侧空洞之后的第一位
        size_t next_idx = idx + right_len + 1;

        // 如果还在当前 block 内，next_idx 必定是下一个存活元素
        if (next_idx < BLOCK_CAPACITY)
        {
            return iterator(blk, next_idx);
        }

        // 超出当前 block，寻找下一个有存活元素的 block
        block* next_blk = blk->next;
        while (next_blk != nullptr && next_blk->active_count == 0)
        {
            next_blk = next_blk->next;
        }

        if (next_blk == nullptr)
        {
            return end();
        }

        // 定位到下一个 block 的首个有效元素（跳过开头的空洞）
        size_t first_valid = 0;
        while (first_valid < BLOCK_CAPACITY && next_blk->skipfield[first_valid] > 0)
        {
            first_valid += next_blk->skipfield[first_valid];
        }

        return iterator(next_blk, first_valid);
    }

    struct slot_location
    {
        block* blk{nullptr};
        size_t idx{0};
    };

    [[nodiscard]]slot_location find_slot_location () const noexcept
    {
        block* curr = head_block_;
        while(curr != nullptr && curr->active_count == BLOCK_CAPACITY) curr = curr->next;
        if(curr == nullptr) { return {nullptr, 0}; }
        return {curr, curr->first_free_idx};
    }
};