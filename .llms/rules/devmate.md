---
oncalls: [mariana_trench]
---

## Build
To build Mariana Trench, run `buck2 build //fbandroid/native/mariana-trench:mariana-trench` in terminal.

## Test
To test Mariana Trench, run `buck2 test //fbandroid/native/mariana-trench/...` in terminal.

## Format
Running `arc f` terminal will automatically format files

## Lint
Running `arc lint` will automatically lint files.

## C++ coding convention notes
- Do not mask outer scope variables within an inner scope.
- Remember to add the delimiters to C++ raw strings if the string itself contains `)"` to avoid confusing the compiler. In most cases, such as our JSON test, a delimiter of `#` would suffice.

## Other notes
- Other than building and testing, please format and lint the code using the commands above.
