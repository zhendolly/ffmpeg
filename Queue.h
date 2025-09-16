#ifndef QUEUE_H
#define QUEUE_H

#include <iostream>

template <typename T>
class Queue {
private:
    struct Node {
        T data;
        Node* next;
        Node(const T& val) : data(val), next(nullptr) {}
    };

    Node* front;
    Node* rear;
    int size;

public:
    Queue() : front(nullptr), rear(nullptr), size(0) {}
    ~Queue() {
        while (!isEmpty()) {
            pop();
        }
    }

    // 禁止拷贝构造和赋值操作
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    void push(const T& value);
    void pop();
    T peek() const;
    bool isEmpty() const;
    int getSize() const;
};

// 模板类的成员函数实现必须放在头文件中
template <typename T>
void Queue<T>::push(const T& value) {
    Node* newNode = new Node(value);
    if (isEmpty()) {
        front = rear = newNode;
    } else {
        rear->next = newNode;
        rear = newNode;
    }
    size++;
}

template <typename T>
void Queue<T>::pop() {
    if (isEmpty()) {
        throw std::runtime_error("Queue is empty");
    }
    Node* temp = front;
    front = front->next;
    delete temp;
    size--;
    if (front == nullptr) {
        rear = nullptr;
    }
}

template <typename T>
T Queue<T>::peek() const {
    if (isEmpty()) {
        throw std::runtime_error("Queue is empty");
    }
    return front->data;
}

template <typename T>
bool Queue<T>::isEmpty() const {
    return size == 0;
}

template <typename T>
int Queue<T>::getSize() const {
    return size;
}

#endif
