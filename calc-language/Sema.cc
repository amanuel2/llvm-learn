#include "Sema.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Support/raw_ostream.h"

////////////////////////////////////////
#pragma mark - Visitors
////////////////////////////////////////

void DeclCheck::visit(Factor &Node) {
  if (Node.getKind() == Factor::Ident) {
    if (Scope.find(Node.getVal()) == Scope.end()) {
      err(Not, Node.getVal());
    }
  }
}

void DeclCheck::visit(BinaryOp &Node) {
  if (Node.getLeft())
    Node.getLeft()->accept(*this);
  else {
    Err = true;
  }
  if (Node.getRight())
    Node.getRight()->accept(*this);
  else
    Err = true;
}

void DeclCheck::visit(With &Node) {

  for (auto it = Node.begin(); it != Node.end(); it++) {
    if (!Scope.insert(*it).second)
      err(Twice, *it);
  }

  if (Node.getExpr())
    Node.getExpr()->accept(*this);
  else
    Err = true;
}

////////////////////////////////////////
#pragma mark - Sema
////////////////////////////////////////

bool Sema::sema(AST *Tree) {
  DeclCheck DC;
  Tree->accept(DC);
  return DC.hasErr();
}