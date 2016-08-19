`iskip` is a set of backend LLVM passes that implements defenses for power
glitching attacks.  The defenses are: load/store verify, branch duplication and
code duplication. `iskip` targets Cortex-M3 ARM arch.

# Requirements

A working build environment is needed. You should be able to build LLVM 4.0 in
your environment.

# Building iskip

1. Run the script `./setup.sh`.  The script will download (and patch) the
   required dependency
2. Run the script `./compile.sh`. The script will compile the LLVM tree and the
   `iskip` LLVM passes. A shared object should be produced
   (`build/lib/LLVMCodeGenHardnening.so`)

# Running iskip

## Running the tests

[cucumber](https://cucumber.io) must be installed. `apt-get install cucumber`
works on Debian based distros.

To run the tests
```sh
cd t && cucumber
```

## Compile different sources
Several ways can be used to compile code:

1. `clang` is the frontend
  ```sh
     ${ISKIP_DIR}/third_party/llvm-install/bin/clang \
        -mllvm -optimize-regalloc=false \
        -mllvm -use-external-regalloc=true \
        -O3 -mllvm -arm-implicit-it=never \
        -Xclang -load -Xclang ${ISKIP_DIR}/build/lib/LLVMCodeGenHardnening.so \
        -target arm-none-eabi -mcpu=cortex-m3 -mfloat-abi=soft -mthumb`
  ```
2. `llc` takes \*.bc files as input and generates assembly files
  ```sh
     ${ISKIP_DIR}/third_party/llvm-install/bin/llc \
       -optimize-regalloc=false -use-external-regalloc=true \
       -arm-force-fast-isel=false -O3 \
       -march=thumb -mcpu=cortex-m3 -float-abi=soft \
       -load=${ISKIP_DIR}/build/lib/LLVMCodeGenHardnening.so \
       in.bc -o out.bc.S
  ```

Run the tests in verbose mode to see what shell commands are generated:
```sh
cd t && V=1 cucumber
```

## Various `iskip` flags

By default, duplicated code will be emitted for _every_ function. However,
there are some useful flags that can be used to customise the way `iskip`
behaves.

```
 -iskip-check-idempotent-verbose         - Enable Check of Indempotent Instructions
 -iskip-deploy-policy                    - Specify when to deploy the passes.
   =any                                  -   Deploy on every function regardless of annotation. This is the default
   =whitelist                            -   Apply the passes only on the whitelist. Do not apply the passes on other functions
   =blacklist                            -   Do not apply the passes on the blacklist. Apply the passes on other functions.
 -iskip-enable-all-duplication-passes    - Enable all passes to achieve ins duplication.
```

Use this command to see all the possible flags
```sh
     ${ISKIP_DIR}/third_party/llvm-install/bin/llc \
       -load=../build/lib/LLVMCodeGenHardnening.so \
       -help-hidden | grep iskip
```

## Annotations

Using annotations, one can enable or disable the effect of passes on specific
functions.

Declare a function with `__attribute__((annotate("armhardnening=true")))` to
include the function in the *whitelist* or with
`__attribute__((annotate("armhardnening=true")))` to include the function in
the *blacklist*. Use the flag `-iskip-deploy-policy` to specify the behavior of
`iskip` in regards to *{white,black}list*.

```C
void my_func(char *)  __attribute__((annotate("armhardnening=true")));
void my_func(char *p) {
  printf("%s\n", p);
}
```

# Disclaimer

Please note that although some of the authors are (or were) employed by Google,
this is not an official Google product.
