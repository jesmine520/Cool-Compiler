class A {
  foo() : Int { 1 };
};

class B inherits A { };

class Main {
  main() : Int { (new B).foo() };
};
