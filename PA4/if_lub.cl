class A { };
class B inherits A { };

class Main {
  main() : A {
    if true then (new B) else (new A) fi
  };
};
