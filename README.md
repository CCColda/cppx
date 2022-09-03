# C++ Extended
This repository provides a Buffer class for managing data easily.

### Customize where data is stored
With `BufferManager`, you can specify how your data can be treated, be it on the heap, stack, or even the GPU.

```cpp
cppx::BufferManager myManager = {
    "myManager", // name
    {
        1, // can be allocated / released
        1  // can be modified
    },
    [](std::size_t size) -> void * { return /* allocate bytes */; },
    [](void *ptr, std::size_t size) -> void { /* deallocate bytes */; }
};
```

### Hello, cppx!

```cpp
#include "cppxBuffer.hpp"
#include <cstdio>

int main() {
	using cppx::Buffer;

	constexpr const char hello[] = "hello, there";
	constexpr const char world[] = "!dlrow";

	auto worldReversed = Buffer::Static((void *)world, sizeof(world))
		.reverse(Buffer::onHeap)
		.erase(0, 1); // remove null terminator

	auto buffer = Buffer::HeapFrom((void *)hello, sizeof(hello));
	buffer.selfErase(buffer.end() - 6, buffer.end());
	buffer.selfInsert(buffer.end(), worldReversed);

	std::printf("%.*s\n%s\n", buffer.size(), buffer.data(), buffer.toString().c_str());
}
```
Will print:
```
hello, world!
0x68656C6C6F2C20776F726C6421
```

### Exceptions
The Exception class holds a call stack and a description of the error.
```cpp
try {
    cppx::Buffer()[10];
} catch (const cppx::Exception& exc) {
    std::printf("Exception: %s\nStack: %s\n", exc.getDescription().c_str(), exc.getCallstackString(7, -7).c_str());
}
```
Will print:
```
Exception: Can't get reference: The buffer is empty
Stack: at([y] 10)
```
