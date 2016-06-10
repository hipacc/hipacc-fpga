//
// Copyright (c) 2014, Saarland University
// Copyright (c) 2014, University of Erlangen-Nuremberg
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

//===------------- HostDataDeps.h - Track data dependencies ---------------===//
//
// This file implements tracking of data dependencies to generate vivado streams
//
//===----------------------------------------------------------------------===//

#include <vector>
#include <iostream>
#include <sstream>

#include <clang/AST/ASTContext.h>
#include <clang/AST/StmtVisitor.h>
#include <clang/Analysis/AnalysisContext.h>
#include <clang/Analysis/Analyses/PostOrderCFGView.h>
#include <clang/Basic/Diagnostic.h>

#include "hipacc/Device/TargetDescription.h"
#include "hipacc/Config/CompilerOptions.h"
#include "hipacc/DSL/CompilerKnownClasses.h"
#include "hipacc/DSL/ClassRepresentation.h"
#include "hipacc/AST/ASTNode.h"

//#define PRINT_DEBUG

namespace clang {
namespace hipacc {

class HostDataDeps;

class DependencyTracker : public StmtVisitor<DependencyTracker> {

  private:
    static const bool DEBUG;

    const CFGBlock *currentBlock;
    ASTContext &Context;
    CompilerKnownClasses &compilerClasses;
    HostDataDeps &dataDeps;

    llvm::DenseMap<ValueDecl *, HipaccAccessor *> accDeclMap_;
    llvm::DenseMap<ValueDecl *, HipaccImage *> imgDeclMap_;
    llvm::DenseMap<ValueDecl *, HipaccIterationSpace *> iterDeclMap_;
    llvm::DenseMap<ValueDecl *, HipaccBoundaryCondition *> bcDeclMap_;

  public:
    DependencyTracker(ASTContext &Context,
                      AnalysisDeclContext &analysisContext,
                      CompilerKnownClasses &compilerClasses,
                      HostDataDeps &dataDeps)
        : Context(Context), compilerClasses(compilerClasses), dataDeps(dataDeps) {
      if (DEBUG) std::cout << "Tracking data dependencies:" << std::endl;
      PostOrderCFGView *POV = analysisContext.getAnalysis<PostOrderCFGView>();
      for (auto it=POV->begin(), ei=POV->end(); it!=ei; ++it) {
        // apply the transfer function for all Stmts in the block.
        const CFGBlock *block = static_cast<const CFGBlock*>(*it);
        for (auto it = block->begin(), ei = block->end(); it != ei; ++it) {
          const CFGElement &elem = *it;
          if (!elem.getAs<CFGStmt>()) continue;

          const Stmt *S = elem.castAs<CFGStmt>().getStmt();
          this->Visit(const_cast<Stmt*>(S));
        }
      }
      if (DEBUG) std::cout << std::endl;
    }

    void VisitDeclStmt(DeclStmt *S);
    void VisitCXXMemberCallExpr(CXXMemberCallExpr *E);
};



class HostDataDeps : public ManagedAnalysis {
  friend class DependencyTracker;

  private:
    static const bool DEBUG;

    // forward declarations
    class Node;

    class Space;
    class Process;

    class Accessor;
    class Image;
    class IterationSpace;
    class BoundaryCondition;
    class Kernel;

    // member variables
    CompilerKnownClasses compilerClasses;
    CompilerOptions compilerOptions;

    llvm::DenseMap<ValueDecl *, Accessor *> accMap_;
    llvm::DenseMap<ValueDecl *, Image *> imgMap_;
    llvm::DenseMap<ValueDecl *, IterationSpace *> iterMap_;
    llvm::DenseMap<ValueDecl *, BoundaryCondition *> bcMap_;
    llvm::DenseMap<ValueDecl *, Kernel *> kernelMap_;

    std::vector<Space*> spaces_;
    std::vector<Process*> processes_;

    unsigned int outId, tmpId;
    std::vector<Node*> schedule;

    // inner class definitions
    class IterationSpace {
      private:
        HipaccIterationSpace *iter;
        Image *image;

      public:
        IterationSpace(HipaccIterationSpace *iter, Image *image)
            : iter(iter), image(image) {
        }

        std::string getName() {
          return iter->getName();
        }

        Image *getImage() {
          return image;
        }
    };

    class Accessor {
      private:
        HipaccAccessor *acc;
        Image *image;
        Space *space;

      public:
        Accessor(HipaccAccessor *acc, Image *image)
            : acc(acc), image(image), space(nullptr) {
        }

        Space *getSpace() {
          return space;
        }

        void setSpace(Space *space) {
          this->space = space;
        }

        std::string getName() {
          return acc->getName();
        }

        Image *getImage() {
          return image;
        }
    };

    class BoundaryCondition {
      private:
        HipaccBoundaryCondition *bc;
        Image* img;

      public:
        BoundaryCondition(HipaccBoundaryCondition *bc, Image* img)
            : bc(bc), img(img) {
        }

        Image* getImage() {
          return img;
        }
    };

    class Image {
      private:
        HipaccImage *img;

      public:
        Image(HipaccImage *img)
            : img(img) {
        }

        std::string getName() {
          return img->getName();
        }

        std::string getTypeStr(size_t ppt) {
          return ASTNode::createVivadoTypeStr(img, ppt);
        }
    };

    class Kernel {
      private:
        std::string name;
        IterationSpace *iter;
        std::vector<Accessor*> accs;

      public:
        Kernel(std::string name, IterationSpace *iter)
            : name(name), iter(iter) {
        }

        std::string getName() {
          return name;
        }

        IterationSpace *getIterationSpace() {
          return iter;
        }

        std::vector<Accessor*> getAccessors() {
          return accs;
        }

        std::vector<Accessor*> getAccessors(Image *image) {
          std::vector<Accessor*> ret;
          for (auto it = accs.begin(); it != accs.end(); ++it) {
            if ((*it)->getImage() == image) {
              ret.push_back(*it);
            }
          }
          return ret;
        }

        void addAccessor(Accessor *acc) {
          accs.push_back(acc);
        }
    };

    class Node {
      private:
        bool space;

      public:
        Node(bool isSpace) {
          space = isSpace;
        }

        bool isSpace() {
          return space;
        }
    };

    class Space : public Node {
      friend class Process;

      private:
        Image *image;
        IterationSpace *iter;
        std::vector<Accessor*> accs_;
        Process *srcProcess;
        std::vector<Process*> dstProcess;

      public:
        std::string stream;
        std::vector<std::string> cpyStreams;

        Space(Image *image)
            : Node(true), image(image), iter(nullptr), srcProcess(nullptr) {
        }

        Image *getImage() {
          return image;
        }

        IterationSpace *getIterationSpace() {
          return iter;
        }

        // only for dump
        std::vector<Accessor*> getAccessors() {
          return accs_;
        }

        Process *getSrcProcess() {
          return srcProcess;
        }

        std::vector<Process*> getDstProcesses() {
          return dstProcess;
        }

        void setSrcProcess(Process *proc) {
          IterationSpace *iter = proc->getKernel()->getIterationSpace();
          assert(iter->getImage() == image && "IterationSpace Image mismatch");
          this->iter = iter;
          srcProcess = proc;
        }

        void addDstProcess(Process *proc) {
          // a single process can have multiple accessors to same image
          std::vector<Accessor*> accs = proc->getKernel()->getAccessors(image);
          assert(accs.size() > 0 && "Accessor Image mismatch");
          for (auto it = accs.begin(); it != accs.end(); ++it) {
            accs_.push_back(*it);
          }
          dstProcess.push_back(proc);
        }

        std::string getTypeStr(size_t ppt) {
          return image->getTypeStr(ppt);
        }
    };

    class Process : public Node {
      private:
        Kernel *kernel;
        Space *outSpace;
        std::vector<Space*> inSpaces;

      public:
        std::vector<std::string> inStreams;
        std::string outStream;

        Process(Kernel *kernel, Space *outSpace)
            : Node(false), kernel(kernel), outSpace(outSpace) {
          outSpace->srcProcess = this;
        }

        Kernel *getKernel() {
          return kernel;
        }

        Space *getOutSpace() {
          return outSpace;
        }

        std::vector<Space*> getInSpaces() {
          return inSpaces;
        }

        void addInputSpace(Space *space) {
          inSpaces.push_back(space);
        }
    };

    template<class T>
    void freeVector(std::vector<T> &vec) {
      for (auto it = vec.begin(); it != vec.end(); ++it) {
        delete (*it);
      }
    }

    template<class T>
    bool findVector(std::vector<T> &vec, T item) {
      return std::find(vec.begin(), vec.end(), item) != vec.end();
    }

    ~HostDataDeps() {
      freeVector(spaces_);
      freeVector(processes_);
    }

    void addImage(ValueDecl *VD, HipaccImage *img);
    void addBoundaryCondition(ValueDecl *BCVD, HipaccBoundaryCondition *BC, ValueDecl *IVD);
    void addKernel(ValueDecl *KVD, ValueDecl *ISVD, std::vector<ValueDecl*> AVDS);
    void addAccessor(ValueDecl *AVD, HipaccAccessor *acc, ValueDecl* IVD);
    void addIterationSpace(ValueDecl *ISVD, HipaccIterationSpace *iter, ValueDecl *IVD);
    void runKernel(ValueDecl *VD);

    void dump(Process *proc);
    void dump(Space *space);
    void dump();

    std::vector<Space*> getInputSpaces();
    std::vector<Space*> getOutputSpaces();
    std::string createStream(Space *s);
    void markProcess(Process *t);
    void markSpace(Space *s);
    void createSchedule();
    std::string declareFifo(std::string type, std::string name);
    std::string getEntrySignature(
        std::map<std::string,std::vector<std::pair<std::string,std::string>>> args,
        bool withTypes=false);
    std::string prettyPrint(
        std::map<std::string,std::vector<std::pair<std::string,std::string>>> args,
        bool print=false);
    std::string getTypeStr(Space *s) {
      // ppt is always 1 if it is OpenCL, because of the CreateChannel macro 
      size_t ppt=1;
      if(!compilerOptions.emitOpenCL()){
        ppt = compilerOptions.getPixelsPerThread();
      }
      return s->getTypeStr(ppt);
    }

  public:
    std::string printFifoDecls(std::string indent);
    bool isStreamForKernel(std::string kernelName, std::string imageName);
    std::string getStreamForKernel(std::string kernelName, std::string imageName);
    std::vector<std::string> getOutputStreamsForKernel(std::string kernelName);
    std::string printEntryDecl(
        std::map<std::string,std::vector<std::pair<std::string,std::string>>> args);
    std::string printEntryCall(
        std::map<std::string,std::vector<std::pair<std::string,std::string>>> args,
        std::string img);
    std::string printEntryDef(
        std::map<std::string,std::vector<std::pair<std::string,std::string>>> args);
    std::string getInputStream(ValueDecl *VD);
    std::string getOutputStream(ValueDecl *VD);
    std::string getStreamDecl(ValueDecl *VD);

    static HostDataDeps *parse(ASTContext &Context,
        AnalysisDeclContext &analysisContext,
        CompilerKnownClasses &compilerClasses,
        CompilerOptions &compilerOptions) {
      static HostDataDeps dataDeps;
      dataDeps.compilerClasses = compilerClasses;
      dataDeps.compilerOptions = compilerOptions;
      DependencyTracker DT(Context, analysisContext, compilerClasses, dataDeps);

      if (DEBUG) {
        std::cout << "Result of data dependency analysis:" << std::endl;
        dataDeps.dump();
        std::cout << std::endl;
      }

      dataDeps.createSchedule();

      return &dataDeps;
    }
};

}
}

