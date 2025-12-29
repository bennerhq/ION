# Ion Language Specification

Ion is a simple imperative programming language designed for high-performance WebAssembly (Wasm) compilation. The compiler must be written in C++ and generate wasm output. When generating C++ code always use clean Code/SOLID-principles

## Design Philosophy

- **Strict & Statically Typed:** Ion enforces strict typing and uses indentation for scoping.
- **Direct Wasm Mapping:** Data types and memory layouts map almost 1:1 with Wasm.
- **No Virtualization:** All method calls are resolved at compile-time (static dispatch), eliminating v-table overhead.
- **64-bit Native:** `int` and `real` map directly to Wasm `i64` and `f64`.

---

## 1. Data Types

- `int`: Signed 64-bit integer.
- `real`: 64-bit floating point.
- `bool`: Boolean (`true` / `false`).
- `string`: Immutable text sequence (length-prefixed in memory).
- `void`: For procedures returning nothing.
- **Arrays:** Denoted by `[]`, e.g., `int[]`, `string[]`.
- **Structures:** User-defined types.

---

## 2. Syntax Rules

- **Blocks:** Defined by indentation (4 spaces or 1 tab).
- **Comments:** Use `#` for single-line comments.
- **Variable Declaration:** `<type> <name> = <value>`
- **Assignment:** `<name> = <value>`
- **Procedures:** `<return type> <name>(<variable list>)`
- **Structs:** `<name>:` (Inheritance: `<name> extends <parent>:`)
- **Imports:** `import <module>` / `import <module> as <alias>` or `import "<path>"` / `import "<path>" as <alias>`

---

## 3. Language Reference & Examples

### Basic Procedures and Math

```Ion
# A simple recursive factorial function
int factorial(int n)
    if n <= 1
        return 1
    else
        return n * factorial(n - 1)

# Main entry point
void main()
    int result = factorial(5)
    print(result)
```

---

### Modules and Imports

Each file is a module. `import <module>` loads `<module>.ion`. Dotted names map to folders. You can also import by path using a string literal; paths are resolved relative to the main file (or absolute if starting with `/`). When importing by path without `as`, the module name defaults to the file stem.

```Ion
# file: std/io.ion
void print_line(string msg)
    print(msg)
```

```Ion
# file: app.ion
import "../std/io" as fido

void main()
    fido.print_line("hello")
```

```Ion
# file: app_simple.ion
import "../std/io"

void main()
    io.print_line("hello")
```

```Ion
# file: app_abs.ion
import "/abs/path/to/lib/math" as math

void main()
    print(math.add(2, 3))
```

---

### CLI Arguments

`main` can be declared as `void main()` or `void main(string[] args)`. When using the `args` form,
the runtime collects command-line arguments (excluding the program name) into a `string[]`.

```Ion
void main(string[] args)
    int i = 0
    while i < args.length()
        print(args[i])
        i = i + 1
```

Example (WASI/wasmtime):

```
# wasmtime app.wat one two three
one
two
three
```

---

### Arrays and Loops

Arrays are fixed-size upon creation using the `new` keyword. Ion does not auto-grow arrays; if you need a larger array, allocate a new one and copy values over manually (the compiler does not insert resizing logic). Arrays are heap-allocated and stored as a pointer to a contiguous block: 8 bytes for length (i64) followed by element data. `length()` reads the header directly, indexing uses `base + 8 + index * element_size`, and there are no bounds checks. Large arrays are limited by available Wasm linear memory; allocations grow memory in 64KiB pages as needed, so very large arrays may fail if memory growth is exhausted.

```Ion
real calculate_average(real[] numbers)
    real sum = 0.0
    int count = numbers.length()
    int i = 0

    while i < count
        sum = sum + numbers[i]
        i = i + 1

    return sum / count

void test_array()
    # Create array of size 3
    real[] my_data = new real[3]
    my_data[0] = 10.5
    my_data[1] = 20.0
    my_data[2] = 5.5

    real avg = calculate_average(my_data)
```

---

### Structures and Nesting

Structures are defined by their name and a colon. They can contain variables and procedures.

```Ion
# Defining a simple vector
vector2:
    real x
    real y

    # Procedure inside struct
    real magnitude()
        # 'this' is implicit or explicit, we'll imply it for simplicity
        return sqrt(x * x + y * y)

# A nested structure
player:
    string username
    int score

    # Nesting vector2 inside player
    vector2 position

    void level_up()
        score = score + 100

void game_logic()
    player p = new player
    p.username = "Hero"
    p.position.x = 10.0
    p.position.y = 5.0

    if p.position.magnitude() > 10.0
        p.level_up()
```

---

### Inheritance (No Virtualization)

No virtualization: upcasting calls parent methods, enabling aggressive inlining.

```Ion
vehicle:
    int speed

    void move()
        print("Vehicle moving")

# Inheritance syntax
car extends vehicle:
    int gears

    # Shadows the parent move, but does not override (no v-table)
    void move()
        print("Car driving fast")

void test_inheritance()
    car c = new car
    c.move() 
    # Output: "Car driving fast"

    # Upcasting
    vehicle v = c
    v.move() 
    # Output: "Vehicle moving" (Fast static dispatch)
```

---

## 4. Compilation to Wasm Strategy

- **Structs:** Flattened into linear memory. Example: `vector2` is 16 bytes (8 for `x`, 8 for `y`).
- **Inheritance:** Child struct appends its fields to the parent's layout. Example: `car` is `[speed (8 bytes)] [gears (8 bytes)]`.
- **Strings/Arrays:** Represented as a pointer to Wasm linear memory. First 8 bytes store the length, followed by the payload.
- **Procedure Calls:** 
    - `call_indirect` is never used for struct methods (no virtualization).
    - All calls use `call <function_index>` for maximum speed.

---

## 4. Testing
- Test sources live in `./testing/code/` and are grouped by category (e.g. `core`, `arrays`, `algorithms`, `format`, `structs`, `imports`, `hard`). Shared fixtures live under `./testing/code/modules` and `./testing/code/std`.
- `./testing/stdout` contains expected output from the example Ion code. Files in this dir have the same name as the Ion code but with the extension `.out`.
- `./testing/stdin` contains CLI parameters for tests that need args. Files in this dir have the same name as the Ion code but with the extension `.in`.
- `./testing/stderr` contains expected compiler errors for negative tests. Files in this dir have the same name as the Ion code but with the extension `.err`.
- `./run_tests.sh` rebuilds the compiler, runs all tests recursively (skipping fixture folders), and reports per-test timing plus total suite time.
- `./debug.sh <path/to/test.ion>` rebuilds the compiler and runs a single test using the same stdin/stdout/stderr rules as the full test runner.
- When building the compiler run `./run_tests.sh` to test the compiler and ensure that `./run_tests.sh` runs with no errors

---

## 6. EBNF Grammar
This grammar assumes a Lexer handles INDENT and DEDENT tokens.

```EBNF
(* --- High Level --- *)
program         ::= { import_decl | declaration } ;
declaration     ::= struct_decl | procedure_decl ;
import_decl     ::= "import" ( module_path | string ) [ "as" identifier ] ;
module_path     ::= identifier { "." identifier } ;
block           ::= INDENT { statement } DEDENT ;

(* --- Types --- *)
type            ::= primitive_type { "[]" } | identifier { "[]" } ;
primitive_type  ::= "int" | "real" | "string" | "bool" ;
return_type     ::= type | "void" ;

(* --- Structures --- *)
struct_decl     ::= identifier [ "extends" identifier ] ":" struct_block ;
struct_block    ::= INDENT { struct_member } DEDENT ;
struct_member   ::= variable_decl | procedure_decl ;

(* --- Procedures --- *)
procedure_decl  ::= return_type identifier "(" [ param_list ] ")" block ;
param_list      ::= param { "," param } ;
param           ::= type identifier ;

(* --- Statements --- *)
statement       ::= variable_decl
                  | assignment_stmt
                  | if_stmt
                  | while_stmt
                  | return_stmt
                  | expression_stmt ;

variable_decl   ::= type identifier "=" expression ;
assignment_stmt ::= primary "=" expression ;
if_stmt         ::= "if" expression block [ "else" block ] ;
while_stmt      ::= "while" expression block ;
return_stmt     ::= "return" [ expression ] ;
expression_stmt ::= expression ;

(* --- Expressions --- *)
expression      ::= logical_or ;
logical_or      ::= logical_and { "or" logical_and } ;
logical_and     ::= equality { "and" equality } ;
equality        ::= relational { ( "==" | "!=" ) relational } ;
relational      ::= additive { ( "<" | ">" | "<=" | ">=" ) additive } ;
additive        ::= multiplicative { ( "+" | "-" ) multiplicative } ;
multiplicative  ::= unary { ( "*" | "/" | "%" ) unary } ;

unary           ::= ( "-" | "!" ) unary | postfix ;
postfix         ::= term { postfix_op } ;
postfix_op      ::= "[" expression "]"       (* Array Access *)
                  | "." identifier           (* Field Access *)
                  | "(" [ arg_list ] ")" ;   (* Function Call *)

arg_list        ::= expression { "," expression } ;
term            ::= literal | identifier | "(" expression ")" | new_expr ;
new_expr        ::= "new" type [ "[" expression "]" ] ;

(* --- Literals --- *)
literal         ::= integer_lit | real_lit | string_lit | boolean_lit ;
boolean_lit     ::= "true" | "false" ;
```
