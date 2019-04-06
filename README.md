# tg
A programming language for generating code.

## Example
Given a generator file like this:
```
generator hello() {
    Hello World!
}

hello();
```
This will print `Hello World!`.

A better example:
```
generator better_enum(enum_name: string, fields: string[]) {
    enum ${enum_name} {
        $for (field in fields) {
            ${field}${,}
        }
    };

    static char const * const ${enum_name}_strings[] = {
        $for (field in fields) {
            "${field}"${,}
        }
    };
}

better_enum("test", ["first", "second", "third"]);
```
This will print:
```c++
enum test {
    first,
    second,
    third
};

static char const * const test_strings[] = {
    "first",
    "second",
    "third"
};
```

## Compiling
You first require a modern C++ compiler that supports C++17.

Easiest building method would be to use GNU make version >= 3.82 and issuing the following command in the root of the repository:
```
make BUILD=release
```
This will build an executable in the build/release directory. Building without `BUILD=release` will create a debug executable by default.
You can change which compiler to use like this:
```
make BUILD=release CXX=gcc-8
```
On windows you should do this from a developer console or use this command:
```
PATH_TO_YOUR_VCVARSALL/vcvarsall.bat x64
make BUILD=release CXX=cl
```

Since this project uses a unity build (one single translation unit) you can also simply build src/main.cpp directly with a similar command to either of these:
```
g++ src/main.cpp -Iextern -Isrc -o build/release/tg.out -std=c++17 -O3
```
```
PATH_TO_YOUR_VCVARSALL/vcvarsall.bat x64
cl src/main.cpp -Iextern -Isrc -Febuild/tg.exe -std:c++17 -O2 -EHs -nologo
```