class A { };
class B inherits A { };
class C inherits A { };

class Main {
  main() : A {
    case (new B) of
      x : B => (new B);
      y : C => (new C);
    esac
  };
};
