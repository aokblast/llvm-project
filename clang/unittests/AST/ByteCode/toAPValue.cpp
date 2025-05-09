#include "../../../lib/AST/ByteCode/Context.h"
#include "../../../lib/AST/ByteCode/Descriptor.h"
#include "../../../lib/AST/ByteCode/Program.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Tooling/Tooling.h"
#include "gtest/gtest.h"

using namespace clang;
using namespace clang::interp;
using namespace clang::ast_matchers;

/// Test the various toAPValue implementations.
TEST(ToAPValue, Pointers) {
  constexpr char Code[] =
      "struct A { bool a; bool z; };\n"
      "struct S {\n"
      "  A a[3];\n"
      "};\n"
      "constexpr S d = {{{true, false}, {false, true}, {false, false}}};\n"
      "constexpr const bool *b = &d.a[1].z;\n"
      "const void *p = (void*)12;\n"
      "const void *nullp = (void*)0;\n"
      "extern int earr[5][5];\n"
      "constexpr const int *arrp = &earr[2][4];\n";

  auto AST = tooling::buildASTFromCodeWithArgs(
      Code, {"-fexperimental-new-constant-interpreter"});

  auto &ASTCtx = AST->getASTContext();
  auto &Ctx = AST->getASTContext().getInterpContext();
  Program &Prog = Ctx.getProgram();

  auto getDecl = [&](const char *Name) -> const ValueDecl * {
    auto Nodes =
        match(valueDecl(hasName(Name)).bind("var"), AST->getASTContext());
    assert(Nodes.size() == 1);
    const auto *D = Nodes[0].getNodeAs<ValueDecl>("var");
    assert(D);
    return D;
  };
  auto getGlobalPtr = [&](const char *Name) -> Pointer {
    const VarDecl *D = cast<VarDecl>(getDecl(Name));
    return Prog.getPtrGlobal(*Prog.getGlobal(D));
  };

  {
    const Pointer &GP = getGlobalPtr("b");
    const Pointer &P = GP.deref<Pointer>();
    ASSERT_TRUE(P.isLive());
    APValue A = P.toAPValue(ASTCtx);
    ASSERT_TRUE(A.isLValue());
    ASSERT_TRUE(A.hasLValuePath());
    const auto &Path = A.getLValuePath();
    ASSERT_EQ(Path.size(), 3u);
    ASSERT_EQ(A.getLValueBase(), getDecl("d"));
    // FIXME: Also test all path elements.
  }

  {
    const ValueDecl *D = getDecl("p");
    ASSERT_NE(D, nullptr);
    const Pointer &GP = getGlobalPtr("p");
    const Pointer &P = GP.deref<Pointer>();
    ASSERT_TRUE(P.isIntegralPointer());
    APValue A = P.toAPValue(ASTCtx);
    ASSERT_TRUE(A.isLValue());
    ASSERT_TRUE(A.getLValueBase().isNull());
    APSInt I;
    bool Success = A.toIntegralConstant(I, D->getType(), AST->getASTContext());
    ASSERT_TRUE(Success);
    ASSERT_EQ(I, 12);
  }

  {
    const ValueDecl *D = getDecl("nullp");
    ASSERT_NE(D, nullptr);
    const Pointer &GP = getGlobalPtr("nullp");
    const Pointer &P = GP.deref<Pointer>();
    ASSERT_TRUE(P.isIntegralPointer());
    APValue A = P.toAPValue(ASTCtx);
    ASSERT_TRUE(A.isLValue());
    ASSERT_TRUE(A.getLValueBase().isNull());
    ASSERT_TRUE(A.isNullPointer());
    APSInt I;
    bool Success = A.toIntegralConstant(I, D->getType(), AST->getASTContext());
    ASSERT_TRUE(Success);
    ASSERT_EQ(I, 0);
  }

  // A multidimensional array.
  {
    const ValueDecl *D = getDecl("arrp");
    ASSERT_NE(D, nullptr);
    const Pointer &GP = getGlobalPtr("arrp").deref<Pointer>();
    APValue A = GP.toAPValue(ASTCtx);
    ASSERT_TRUE(A.isLValue());
    ASSERT_TRUE(A.hasLValuePath());
    ASSERT_EQ(A.getLValuePath().size(), 2u);
    ASSERT_EQ(A.getLValuePath()[0].getAsArrayIndex(), 2u);
    ASSERT_EQ(A.getLValuePath()[1].getAsArrayIndex(), 4u);
    ASSERT_EQ(A.getLValueOffset().getQuantity(), 56u);
    ASSERT_TRUE(
        GP.atIndex(0).getFieldDesc()->getElemQualType()->isIntegerType());
  }
}

TEST(ToAPValue, FunctionPointers) {
  constexpr char Code[] = " constexpr bool foo() { return true; }\n"
                          " constexpr bool (*func)() = foo;\n"
                          " constexpr bool (*nullp)() = nullptr;\n";

  auto AST = tooling::buildASTFromCodeWithArgs(
      Code, {"-fexperimental-new-constant-interpreter"});

  auto &ASTCtx = AST->getASTContext();
  auto &Ctx = AST->getASTContext().getInterpContext();
  Program &Prog = Ctx.getProgram();

  auto getDecl = [&](const char *Name) -> const ValueDecl * {
    auto Nodes =
        match(valueDecl(hasName(Name)).bind("var"), AST->getASTContext());
    assert(Nodes.size() == 1);
    const auto *D = Nodes[0].getNodeAs<ValueDecl>("var");
    assert(D);
    return D;
  };

  auto getGlobalPtr = [&](const char *Name) -> Pointer {
    const VarDecl *D = cast<VarDecl>(getDecl(Name));
    return Prog.getPtrGlobal(*Prog.getGlobal(D));
  };

  {
    const Pointer &GP = getGlobalPtr("func");
    const Pointer &FP = GP.deref<Pointer>();
    ASSERT_FALSE(FP.isZero());
    APValue A = FP.toAPValue(ASTCtx);
    ASSERT_TRUE(A.hasValue());
    ASSERT_TRUE(A.isLValue());
    ASSERT_TRUE(A.hasLValuePath());
    const auto &Path = A.getLValuePath();
    ASSERT_EQ(Path.size(), 0u);
    ASSERT_FALSE(A.getLValueBase().isNull());
    ASSERT_EQ(A.getLValueBase().dyn_cast<const ValueDecl *>(), getDecl("foo"));
  }

  {
    const ValueDecl *D = getDecl("nullp");
    ASSERT_NE(D, nullptr);
    const Pointer &GP = getGlobalPtr("nullp");
    const auto &P = GP.deref<FunctionPointer>();
    APValue A = P.toAPValue(ASTCtx);
    ASSERT_TRUE(A.isLValue());
    ASSERT_TRUE(A.getLValueBase().isNull());
    ASSERT_TRUE(A.isNullPointer());
    APSInt I;
    bool Success = A.toIntegralConstant(I, D->getType(), AST->getASTContext());
    ASSERT_TRUE(Success);
    ASSERT_EQ(I, 0);
  }
}

TEST(ToAPValue, FunctionPointersC) {
  // NB: The declaration of func2 is useless, but it makes us register a global
  // variable for func.
  constexpr char Code[] = "const int (* const func)(int *) = (void*)17;\n"
                          "const int (*func2)(int *) = func;\n";
  auto AST = tooling::buildASTFromCodeWithArgs(
      Code, {"-x", "c", "-fexperimental-new-constant-interpreter"});

  auto &ASTCtx = AST->getASTContext();
  auto &Ctx = AST->getASTContext().getInterpContext();
  Program &Prog = Ctx.getProgram();

  auto getDecl = [&](const char *Name) -> const ValueDecl * {
    auto Nodes =
        match(valueDecl(hasName(Name)).bind("var"), AST->getASTContext());
    assert(Nodes.size() == 1);
    const auto *D = Nodes[0].getNodeAs<ValueDecl>("var");
    assert(D);
    return D;
  };

  auto getGlobalPtr = [&](const char *Name) -> Pointer {
    const VarDecl *D = cast<VarDecl>(getDecl(Name));
    return Prog.getPtrGlobal(*Prog.getGlobal(D));
  };

  {
    const ValueDecl *D = getDecl("func");
    const Pointer &GP = getGlobalPtr("func");
    ASSERT_TRUE(GP.isLive());
    const Pointer &FP = GP.deref<Pointer>();
    ASSERT_FALSE(FP.isZero());
    APValue A = FP.toAPValue(ASTCtx);
    ASSERT_TRUE(A.hasValue());
    ASSERT_TRUE(A.isLValue());
    const auto &Path = A.getLValuePath();
    ASSERT_EQ(Path.size(), 0u);
    ASSERT_TRUE(A.getLValueBase().isNull());
    APSInt I;
    bool Success = A.toIntegralConstant(I, D->getType(), AST->getASTContext());
    ASSERT_TRUE(Success);
    ASSERT_EQ(I, 17);
  }
}

TEST(ToAPValue, MemberPointers) {
  constexpr char Code[] = "struct S {\n"
                          "  int m, n;\n"
                          "};\n"
                          "constexpr int S::*pm = &S::m;\n"
                          "constexpr int S::*nn = nullptr;\n";

  auto AST = tooling::buildASTFromCodeWithArgs(
      Code, {"-fexperimental-new-constant-interpreter"});

  auto &ASTCtx = AST->getASTContext();
  auto &Ctx = AST->getASTContext().getInterpContext();
  Program &Prog = Ctx.getProgram();

  auto getDecl = [&](const char *Name) -> const ValueDecl * {
    auto Nodes =
        match(valueDecl(hasName(Name)).bind("var"), AST->getASTContext());
    assert(Nodes.size() == 1);
    const auto *D = Nodes[0].getNodeAs<ValueDecl>("var");
    assert(D);
    return D;
  };

  auto getGlobalPtr = [&](const char *Name) -> Pointer {
    const VarDecl *D = cast<VarDecl>(getDecl(Name));
    return Prog.getPtrGlobal(*Prog.getGlobal(D));
  };

  {
    const Pointer &GP = getGlobalPtr("pm");
    ASSERT_TRUE(GP.isLive());
    const MemberPointer &FP = GP.deref<MemberPointer>();
    APValue A = FP.toAPValue(ASTCtx);
    ASSERT_EQ(A.getMemberPointerDecl(), getDecl("m"));
    ASSERT_EQ(A.getKind(), APValue::MemberPointer);
  }

  {
    const Pointer &GP = getGlobalPtr("nn");
    ASSERT_TRUE(GP.isLive());
    const MemberPointer &NP = GP.deref<MemberPointer>();
    ASSERT_TRUE(NP.isZero());
    APValue A = NP.toAPValue(ASTCtx);
    ASSERT_EQ(A.getKind(), APValue::MemberPointer);
  }
}

/// Compare outputs between the two interpreters.
TEST(ToAPValue, Comparison) {
  constexpr char Code[] =
      "constexpr int GI = 12;\n"
      "constexpr const int *PI = &GI;\n"
      "struct S{int a; };\n"
      "constexpr S GS[] = {{}, {}};\n"
      "constexpr const S* OS = GS + 1;\n"
      "constexpr S DS = *OS;\n"
      "constexpr int IA[2][2][2] = {{{1, 2}, {3, 4}}, {{5, 6}, {7, 8}}};\n"
      "constexpr const int *PIA = IA[1][1];\n";

  for (const char *Arg : {"", "-fexperimental-new-constant-interpreter"}) {
    auto AST = tooling::buildASTFromCodeWithArgs(Code, {Arg});

    auto getDecl = [&](const char *Name) -> const VarDecl * {
      auto Nodes =
          match(valueDecl(hasName(Name)).bind("var"), AST->getASTContext());
      assert(Nodes.size() == 1);
      const auto *D = Nodes[0].getNodeAs<ValueDecl>("var");
      assert(D);
      return cast<VarDecl>(D);
    };

    {
      const VarDecl *GIDecl = getDecl("GI");
      const APValue *V = GIDecl->evaluateValue();
      ASSERT_TRUE(V->isInt());
    }

    {
      const VarDecl *GIDecl = getDecl("GI");
      const VarDecl *PIDecl = getDecl("PI");
      const APValue *V = PIDecl->evaluateValue();
      ASSERT_TRUE(V->isLValue());
      ASSERT_TRUE(V->hasLValuePath());
      ASSERT_EQ(V->getLValueBase().get<const ValueDecl *>(), GIDecl);
      ASSERT_EQ(V->getLValuePath().size(), 0u);
    }

    {
      const APValue *V = getDecl("OS")->evaluateValue();
      ASSERT_TRUE(V->isLValue());
      ASSERT_EQ(V->getLValueOffset().getQuantity(), (unsigned)sizeof(int));
      ASSERT_TRUE(V->hasLValuePath());
      ASSERT_EQ(V->getLValuePath().size(), 1u);
      ASSERT_EQ(V->getLValuePath()[0].getAsArrayIndex(), 1u);
    }

    {
      const APValue *V = getDecl("DS")->evaluateValue();
      ASSERT_TRUE(V->isStruct());
    }
    {
      const APValue *V = getDecl("PIA")->evaluateValue();
      ASSERT_TRUE(V->isLValue());
      ASSERT_TRUE(V->hasLValuePath());
      ASSERT_EQ(V->getLValuePath().size(), 3u);
      ASSERT_EQ(V->getLValuePath()[0].getAsArrayIndex(), 1u);
      ASSERT_EQ(V->getLValuePath()[1].getAsArrayIndex(), 1u);
      ASSERT_EQ(V->getLValuePath()[2].getAsArrayIndex(), 0u);
      ASSERT_EQ(V->getLValueOffset().getQuantity(), (unsigned)sizeof(int) * 6);
    }
  }
}
