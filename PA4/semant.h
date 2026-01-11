#ifndef SEMANT_H_
#define SEMANT_H_

#include <assert.h>
#include <iostream>  
#include "cool-tree.h"
#include "stringtab.h"
#include "symtab.h"
#include "list.h"

#define TRUE 1
#define FALSE 0

class ClassTable;
typedef ClassTable *ClassTableP;

// This is a structure that may be used to contain the semantic
// information such as the inheritance graph.  You may use it or not as
// you like: it is only here to provide a container for the supplied
// methods.

class ClassTable {
private:
  // 新增：保存 program 的 classes，方便查方法
  Classes classes;

  int semant_errors;
  void install_basic_classes();
  ostream& error_stream;

public:
  Symbol filename_of(Symbol class_name);
  Symbol parent_of(Symbol class_name);

  bool conforms(Symbol child, Symbol parent);
  Symbol lub(Symbol a, Symbol b);

  ClassTable(Classes);
  int errors() { return semant_errors; }
  
  // 新增：查找某个类里某个方法（先不走继承链，后面再扩展）
  method_class* lookup_method(Symbol class_name, Symbol method_name);

  ostream& semant_error();
  ostream& semant_error(Class_ c);
  ostream& semant_error(Symbol filename, tree_node *t);
};


#endif

