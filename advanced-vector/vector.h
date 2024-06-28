#pragma once

#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
            : buffer_(Allocate(capacity))
            , capacity_(capacity) {
    }

    RawMemory(const RawMemory& other) = delete;
    RawMemory& operator=(const RawMemory& other) = delete;

    RawMemory(RawMemory&& other) noexcept :
            buffer_(std::exchange(other.buffer_, nullptr)),
            capacity_(std::exchange(other.capacity_, 0))
    {}

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            this->Swap(rhs);
        }

        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    Vector() = default;

    explicit Vector(size_t size) :
            data_(size),
            size_(size){
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other) :
            data_(other.size_),
            size_(other.size_){
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    Vector(Vector&& other) :
            data_(std::move(other.data_)),
            size_(std::exchange(other.size_, 0))
    {}

    ~Vector(){
        std::destroy_n(data_.GetAddress(), size_);
    }

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept{
        return data_.GetAddress();
    }
    iterator end() noexcept{
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept{
        return data_.GetAddress();
    }
    const_iterator end() const noexcept{
        return data_.GetAddress() + size_;
    }
    const_iterator cbegin() const noexcept{
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept{
        return data_.GetAddress() + size_;
    }




    void CopyVector(const Vector& other){
        std::copy(other.data_.GetAddress(),
                  other.data_.GetAddress() + std::min(size_, other.size_),
                  data_.GetAddress());

        if (size_ <= other.size_) {
            std::uninitialized_copy_n(other.data_.GetAddress() + size_,
                                      other.size_ - size_,
                                      data_.GetAddress() + size_);
        }else{
            std::destroy_n(data_.GetAddress() + other.size_,
                           size_ - other.size_);
        }
        size_ = other.size_;
    }
    Vector& operator=(const Vector& other) {
        if (this != &other) {
            if (other.size_ <= data_.Capacity()) {
               CopyVector(other);
            } else {
                Vector other_copy(other);
                Swap(other_copy);
            }
        }

        return *this;
    }

    Vector& operator=(Vector&& other) noexcept {
        if(this != &other) {
            Swap(other);
        }
        return *this;
    }

    void Swap(Vector& other) noexcept {
        if(this != &other) {
            data_.Swap(other.data_);
            std::swap(size_, other.size_);
        }
    }

    void Reserve(size_t new_capacity){
        if(data_.Capacity() >= new_capacity){
            return;
        }

        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size){
        Reserve(new_size);
        if(new_size > size_){
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }else{
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        size_ = new_size;
    }

    template <typename Obj>
    void PushBack(Obj&& obj){
        if(size_ >= data_.Capacity()){
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data.GetAddress() + size_) T(std::forward<Obj>(obj));

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }else{
            new(data_.GetAddress() + size_) T(std::forward<Obj>(obj));
        }
        size_ ++;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args){
        return *Emplace(end(), std::forward<Args>(args)...);
    }

    void PopBack(){
        if(size_ > 0){
            std::destroy_at(data_.GetAddress() + size_ - 1);
            size_ --;
        }
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args){
        assert(pos >= begin() && pos <= end());
        auto position = pos - begin();

        if(size_ < data_.Capacity()){
            try{
                if(pos == end()){
                    new (end()) T(std::forward<Args>(args)...);
                }else{
                    T new_item(std::forward<Args>(args)...);
                    new (end()) T(std::forward<T>(data_[size_ - 1]));
                    std::move_backward(begin() + position, end() - 1, end());
                    *(begin() + position) = std::forward<T>(new_item);
                }
            }catch (...){
                operator delete(end());
                throw;
            }
        }else{
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data.GetAddress() + position) T(std::forward<Args>(args)...);

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(begin(), position, new_data.GetAddress());
                std::uninitialized_move_n(begin() + position, size_ - position, new_data.GetAddress() + position + 1);
            } else {
                std::uninitialized_copy_n(begin(), position, new_data.GetAddress());
                std::uninitialized_copy_n(begin() + position, size_ - position, new_data.GetAddress() + position + 1);
            }

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        size_++;
        return begin() + position;
    }

    iterator Insert(const_iterator pos, const T& value){
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value){
        return Emplace(pos, std::move(value));
    }

    iterator Erase(const_iterator pos) {
        assert(pos >= begin() && pos < end());
        auto position = pos - begin();

        std::move(begin() + position + 1, end(), begin() + position);
        std::destroy_at(end() - 1);
        size_--;

        return (begin() + position);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    static T* AllocateN(size_t n){
        if(n > 0) {
            return static_cast<T *>(operator new(n * sizeof(T)));
        }else{
            return nullptr;
        }
    }

    static void Deallocate(T* data) noexcept {
        operator delete (data);
    }

    static void Destroy(T* data) noexcept{
        data->~T();
    }

    static void DestroyN(T* data, size_t n) noexcept{
        for(size_t i = 0; i < n; i++){
            Destroy(data + i);
        }
    }

    static void CopyConstruct(T* data, const T& elem) {
        new (data) T(elem);
    }
};