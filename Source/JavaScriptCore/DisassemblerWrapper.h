#if defined(DISASSEMBLE)
#ifndef DISASSEMBERWRAPPER_H
#define DISASSEMBERWRAPPER_H
#include "DisassemblerARM.h"

namespace JSC {

    class DisassemblerWrapper {
        public:
            explicit DisassemblerWrapper(CodeBlock *codeblock, ExecState *execstate, LinkBuffer *linkbuffer, Vector<JSC::AbstractMacroAssembler<JSC::ARMAssembler>::Label, 0u> labels)
                : codeblock_(codeblock), execstate_(execstate), linkbuffer_(linkbuffer), labels_(labels)
            {}

            void printSourceName()	{
                printf("\n\n%s:%d\n", codeblock_->ownerExecutable()->sourceURL().utf8().data(), codeblock_->ownerExecutable()->lineNo());

            }

            void printByteCode(Vector<Instruction>::const_iterator begin, Vector<Instruction>::const_iterator it) {
                codeblock_->dump(execstate_, begin, it);
            }

            void printAssemble(int begin, int end) {
                DISASM::byte *from = getByteCodeAssembleLocation(begin);
                DISASM::byte *to = getByteCodeAssembleLocation(end);
                DISASM::Disassembler::disassemble(from, to);
            }

            static void printAssemble(DISASM::byte *from, DISASM::byte *to) {
                DISASM::Disassembler::disassemble(from, to);
            }

            void PrintPrologue() {
                DISASM::byte *from = getCode();
                DISASM::byte *to = getByteCodeAssembleLocation(0);
                printf("[ Prologue: ]\n");
                DISASM::Disassembler::disassemble(from, to);
            }

            void PrintData() {
                printf("[ Data: ]\n");
                DISASM::byte *from = getByteCodeAssembleLocation(codeblock_->instructions().size());
                DISASM::byte *to = getCode() + linkbuffer_->getSize();
                for (DISASM::byte *it = from; it != to; it = it + 4)
                    DISASM::Disassembler::DataContent(it);
            }

            void printAll() {
                // print file name and line number
                printSourceName();

                PrintPrologue();
                // foreach byte code printout info
                Vector<Instruction>::const_iterator begin = codeblock_->instructions().begin();
                Vector<Instruction>::const_iterator it = begin;
                unsigned instructionCount = codeblock_->instructions().size();
                for (unsigned i = 0; i < instructionCount; ) {
                    // print bytecode info
                    printByteCode(begin, it);

                    // print native code info
                    int insnsize = getByteCodeLength(it);
                    printAssemble(i, i+insnsize);

                    i += insnsize;
                    it += insnsize;
                }
                if (!getenv("NODATA"))
                    PrintData();
            }
        private:
            CodeBlock *codeblock_;
            ExecState *execstate_;
            LinkBuffer *linkbuffer_;
            Vector<JSC::AbstractMacroAssembler<JSC::ARMAssembler>::Label, 0u> labels_;

            int getByteCodeLength(Vector<Instruction>::const_iterator it)
            {
                return opcodeLengths[(it)->u.opcode];
            }

            DISASM::byte *getByteCodeAssembleLocation(int count) {
                return reinterpret_cast<DISASM::byte*>(linkbuffer_->locationOf(labels_[count]).executableAddress());
            }

            DISASM::byte *getCode() {
                return reinterpret_cast<DISASM::byte*>(linkbuffer_->getCode());
            }
    };


} //namespace JSC

#endif
#endif
