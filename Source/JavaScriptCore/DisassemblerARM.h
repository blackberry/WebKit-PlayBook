#if defined(DISASSEMBLE)
#ifndef DISASSEMBERARM_H
#define DISASSEMBERARM_H
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cassert>

namespace DISASM {
    // Decoder decodes and disassembles instructions into an output buffer.
    // It uses the converter to convert register names and call destinations into
    // more informative description.

    typedef signed int int32_t;
    typedef unsigned int uint32_t;
    typedef unsigned long long uint64_t;
    typedef unsigned char byte;
    typedef int32_t instr_t;


    // Number of registers in normal ARM mode.
    static const int kNumRegisters = 16;


    // VFP support.
    static const int kNumVFPSingleRegisters = 32;
    static const int kNumVFPDoubleRegisters = 16;
    static const int kNumVFPRegisters =
        kNumVFPSingleRegisters + kNumVFPDoubleRegisters;


    // PC is register 15.
    static const int kPCRegister = 15;
    static const int kNoRegister = -1;


    // These condition names are defined in a way to match the native disassembler
    // formatting. See for example the command "objdump -d <binary file>".
    static const char* cond_names[16] = {
        //static const char* cond_names[max_condition] = {
        "eq", "ne", "cs" , "cc" , "mi" , "pl" , "vs" , "vc" ,
        "hi", "ls", "ge", "lt", "gt", "le", "", "invalid"
    };


    // These shift names are defined in a way to match the native disassembler
    // formatting. See for example the command "objdump -d <binary file>".
    //static const char* shift_names[max_shift] = {
    static const char* shift_names[4] = {
        "lsl", "lsr", "asr", "ror"
    };


    enum Condition {
        no_condition = -1,
        EQ =  0,  // equal
        NE =  1,  // not equal
        CS =  2,  // carry set/unsigned higher or same
        CC =  3,  // carry clear/unsigned lower
        MI =  4,  // minus/negative
        PL =  5,  // plus/positive or zero
        VS =  6,  // overflow
        VC =  7,  // no overflow
        HI =  8,  // unsigned higher
        LS =  9,  // unsigned lower or same
        GE = 10,  // signed greater than or equal
        LT = 11,  // signed less than
        GT = 12,  // signed greater than
        LE = 13,  // signed less than or equal
        AL = 14,  // always (unconditional)
        special_condition = 15,  // special condition (refer to section A3.2.1)
        max_condition = 16
    };


    // Opcodes for Data-processing instructions (instructions with a type 0 and 1)
    // as defined in section A3.4
    enum Opcode {
        no_operand = -1,
        AND =  0,  // Logical AND
        EOR =  1,  // Logical Exclusive OR
        SUB =  2,  // Subtract
        RSB =  3,  // Reverse Subtract
        ADD =  4,  // Add
        ADC =  5,  // Add with Carry
        SBC =  6,  // Subtract with Carry
        RSC =  7,  // Reverse Subtract with Carry
        TST =  8,  // Test
        TEQ =  9,  // Test Equivalence
        CMP = 10,  // Compare
        CMN = 11,  // Compare Negated
        ORR = 12,  // Logical (inclusive) OR
        MOV = 13,  // Move
        BIC = 14,  // Bit Clear
        MVN = 15,  // Move Not
        max_operand = 16
    };


    // The bits for bit 7-4 for some type 0 miscellaneous instructions.
    enum MiscInstructionsBits74 {
        // With bits 22-21 01.
        BX   =  1,
        BXJ  =  2,
        BLX  =  3,
        BKPT =  7,

        // With bits 22-21 11.
        CLZ  =  1
    };


    // Shifter types for Data-processing operands as defined in section A5.1.2.
    enum Shift {
        no_shift = -1,
        LSL = 0,  // Logical shift left
        LSR = 1,  // Logical shift right
        ASR = 2,  // Arithmetic shift right
        ROR = 3,  // Rotate right
        max_shift = 4
    };


    // Special Software Interrupt codes when used in the presence of the ARM
    // simulator.
    enum SoftwareInterruptCodes {
        // transition to C code
        call_rt_redirected = 0x10,
        // break point
        break_point = 0x20
    };


    // Type of VFP register. Determines register encoding.
    enum VFPRegPrecision {
        kSinglePrecision = 0,
        kDoublePrecision = 1
    };


    // The class Instr enables access to individual fields defined in the ARM
    // architecture instruction set encoding as described in figure A3-1.
    //
    // Example: Test whether the instruction at ptr does set the condition code
    // bits.
    //
    // bool InstructionSetsConditionCodes(byte* ptr) {
    //   Instr* instr = Instr::At(ptr);
    //   int type = instr->TypeField();
    //   return ((type == 0) || (type == 1)) && instr->HasS();
    // }
    //
    class Instr {
        public:
            enum {
                kInstrSize = 4,
                kInstrSizeLog2 = 2,
                kPCReadOffset = 8
            };

            // Get the raw instruction bits.
            inline instr_t InstructionBits() const {
                return *reinterpret_cast<const instr_t*>(this);
            }

            // Set the raw instruction bits to value.
            inline void SetInstructionBits(instr_t value) {
                *reinterpret_cast<instr_t*>(this) = value;
            }

            // Read one particular bit out of the instruction bits.
            inline int Bit(int nr) const {
                return (InstructionBits() >> nr) & 1;
            }

            // Read a bit field out of the instruction bits.
            inline int Bits(int hi, int lo) const {
                return (InstructionBits() >> lo) & ((2 << (hi - lo)) - 1);
            }


            // Accessors for the different named fields used in the ARM encoding.
            // The naming of these accessor corresponds to figure A3-1.
            // Generally applicable fields
            inline Condition ConditionField() const {
                return static_cast<Condition>(Bits(31, 28));
            }
            inline int TypeField() const { return Bits(27, 25); }

            inline int RnField() const { return Bits(19, 16); }
            inline int RdField() const { return Bits(15, 12); }

            inline int CoprocessorField() const { return Bits(11, 8); }
            // Support for VFP.
            // Vn(19-16) | Vd(15-12) |  Vm(3-0)
            inline int VnField() const { return Bits(19, 16); }
            inline int VmField() const { return Bits(3, 0); }
            inline int VdField() const { return Bits(15, 12); }
            inline int NField() const { return Bit(7); }
            inline int MField() const { return Bit(5); }
            inline int DField() const { return Bit(22); }
            inline int RtField() const { return Bits(15, 12); }
            inline int PField() const { return Bit(24); }
            inline int UField() const { return Bit(23); }
            inline int Opc1Field() const { return (Bit(23) << 2) | Bits(21, 20); }
            inline int Opc2Field() const { return Bits(19, 16); }
            inline int Opc3Field() const { return Bits(7, 6); }
            inline int SzField() const { return Bit(8); }
            inline int VLField() const { return Bit(20); }
            inline int VCField() const { return Bit(8); }
            inline int VAField() const { return Bits(23, 21); }
            inline int VBField() const { return Bits(6, 5); }
            inline int VFPNRegCode(VFPRegPrecision pre) {
                return VFPGlueRegCode(pre, 16, 7);
            }
            inline int VFPMRegCode(VFPRegPrecision pre) {
                return VFPGlueRegCode(pre, 0, 5);
            }
            inline int VFPDRegCode(VFPRegPrecision pre) {
                return VFPGlueRegCode(pre, 12, 22);
            }

            // Fields used in Data processing instructions
            inline Opcode OpcodeField() const {
                return static_cast<Opcode>(Bits(24, 21));
            }
            inline int SField() const { return Bit(20); }
            // with register
            inline int RmField() const { return Bits(3, 0); }
            inline Shift ShiftField() const { return static_cast<Shift>(Bits(6, 5)); }
            inline int RegShiftField() const { return Bit(4); }
            inline int RsField() const { return Bits(11, 8); }
            inline int ShiftAmountField() const { return Bits(11, 7); }
            // with immediate
            inline int RotateField() const { return Bits(11, 8); }
            inline int Immed8Field() const { return Bits(7, 0); }
            inline int Immed4Field() const { return Bits(19, 16); }
            inline int ImmedMovwMovtField() const {
                return Immed4Field() << 12 | Offset12Field(); }

            // Fields used in Load/Store instructions
            inline int PUField() const { return Bits(24, 23); }
            inline int  BField() const { return Bit(22); }
            inline int  WField() const { return Bit(21); }
            inline int  LField() const { return Bit(20); }
            // with register uses same fields as Data processing instructions above
            // with immediate
            inline int Offset12Field() const { return Bits(11, 0); }
            // multiple
            inline int RlistField() const { return Bits(15, 0); }
            // extra loads and stores
            inline int SignField() const { return Bit(6); }
            inline int HField() const { return Bit(5); }
            inline int ImmedHField() const { return Bits(11, 8); }
            inline int ImmedLField() const { return Bits(3, 0); }

            // Fields used in Branch instructions
            inline int LinkField() const { return Bit(24); }
            inline int SImmed24Field() const { return ((InstructionBits() << 8) >> 8); }

            // Fields used in Software interrupt instructions
            inline SoftwareInterruptCodes SwiField() const {
                return static_cast<SoftwareInterruptCodes>(Bits(23, 0));
            }

            // Test for special encodings of type 0 instructions (extra loads and stores,
            // as well as multiplications).
            inline bool IsSpecialType0() const { return (Bit(7) == 1) && (Bit(4) == 1); }

            // Test for miscellaneous instructions encodings of type 0 instructions.
            inline bool IsMiscType0() const { return (Bit(24) == 1)
                && (Bit(23) == 0)
                    && (Bit(20) == 0)
                    && (Bit(7) == 0); }

            // Special accessors that test for existence of a value.
            inline bool HasS()    const { return SField() == 1; }
            inline bool HasB()    const { return BField() == 1; }
            inline bool HasW()    const { return WField() == 1; }
            inline bool HasL()    const { return LField() == 1; }
            inline bool HasU()    const { return UField() == 1; }
            inline bool HasSign() const { return SignField() == 1; }
            inline bool HasH()    const { return HField() == 1; }
            inline bool HasLink() const { return LinkField() == 1; }

            // Decoding the double immediate in the vmov instruction.
            double DoubleImmedVmov() const
            {
                // Reconstruct a double from the immediate encoded in the vmov instruction.
                //
                //   instruction: [xxxxxxxx,xxxxabcd,xxxxxxxx,xxxxefgh]
                //   double: [aBbbbbbb,bbcdefgh,00000000,00000000,
                //            00000000,00000000,00000000,00000000]
                //
                // where B = ~b. Only the high 16 bits are affected.
                uint64_t high16;
                high16  = (Bits(17, 16) << 4) | Bits(3, 0);   // xxxxxxxx,xxcdefgh.
                high16 |= (0xff * Bit(18)) << 6;              // xxbbbbbb,bbxxxxxx.
                high16 |= (Bit(18) ^ 1) << 14;                // xBxxxxxx,xxxxxxxx.
                high16 |= Bit(19) << 15;                      // axxxxxxx,xxxxxxxx.

                uint64_t imm = high16 << 48;
                double d;
                memcpy(&d, &imm, 8);
                return d;
            }
            // Instructions are read of out a code stream. The only way to get a
            // reference to an instruction is to convert a pointer. There is no way
            // to allocate or create instances of class Instr.
            // Use the At(pc) function to create references to Instr.
            static Instr* At(byte* pc) { return reinterpret_cast<Instr*>(pc); }

        private:
            // Join split register codes, depending on single or double precision.
            // four_bit is the position of the least-significant bit of the four
            // bit specifier. one_bit is the position of the additional single bit
            // specifier.
            inline int VFPGlueRegCode(VFPRegPrecision pre, int four_bit, int one_bit) {
                if (pre == kSinglePrecision)
                    return (Bits(four_bit + 3, four_bit) << 1) | Bit(one_bit);
                return (Bit(one_bit) << 4) | Bits(four_bit + 3, four_bit);
            }

            // We need to prevent the creation of instances of class Instr.
            // DISALLOW_IMPLICIT_CONSTRUCTORS(Instr);
            Instr() {}
    }; // class Instr


    // Helper functions for converting between register numbers and names.
    class Registers {
        public:
            // Return the name of the register.
            static const char* Name(int reg);

            // Lookup the register number for the name provided.
            static int Number(const char* name);

            struct RegisterAlias {
                int reg;
                const char* name;
            };

        private:
            static const char* names_[kNumRegisters];
            static const RegisterAlias aliases_[];
    }; // class Registers

    // Helper functions for converting between VFP register numbers and names.
    class VFPRegisters {
        public:
            // Return the name of the register.
            static const char* Name(int reg, bool is_double);

            // Lookup the register number for the name provided.
            // Set flag pointed by is_double to true if register
            // is double-precision.
            static int Number(const char* name, bool* is_double);

        private:
            static const char* names_[kNumVFPRegisters];
    }; // class VFPRegisters


    class Disassembler {
        public:
            static void disassemble(unsigned inst){}
            static void disassemble(byte *addr) {
                InstructionDecode(reinterpret_cast<Instr*>(addr));
            }
            static void disassemble(byte *from, byte *to) 	{
                byte *it;

                for(it = from; it != to; it = it+4)
                    disassemble(it);
            }
            static void DataContent(byte *addr) {
                Instr *instr = reinterpret_cast<Instr*>(addr);
                printf("\t%p\t.word\t%8x\n", instr, instr->InstructionBits());
            }

        private:
            static void Unknown(Instr* instr){}
            static void UNREACHABLE(){}
            static void UNIMPLEMENTED(){}
            static const int buffer_length_=64;
            static char out_buffer_[buffer_length_];
            static int out_buffer_pos_;


            static void InstructionDecode(Instr *instr) {
                if(!getenv("NOADDR"))
                    printf("\t%p\t%8x", instr, instr->InstructionBits());
                printf("\t");
                switch (instr->TypeField()) {
                    case 0:
                    case 1: {
                                DecodeType01(instr);
                                break;
                            }
                    case 2: {
                                DecodeType2(instr);
                                break;
                            }
                    case 3: {
                                DecodeType3(instr);
                                break;
                            }
                    case 4: {
                                DecodeType4(instr);
                                break;
                            }
                    case 5: {
                                DecodeType5(instr);
                                break;
                            }
                    case 6: {
                                DecodeType6(instr);
                                break;
                            }
                    case 7: {
                                DecodeType7(instr);
                                break;
                            }
                    default: {
                                 // The type field is 3-bits in the ARM encoding.
                                 UNREACHABLE();
                                 printf("\tundef");
                                 break;
                             }
                }
                printf("\n");
            }

            static void DecodeType01(Instr* instr) {
                int type = instr->TypeField();
                if ((type == 0) && instr->IsSpecialType0()) {
                    // multiply instruction or extra loads and stores
                    if (instr->Bits(7, 4) == 9) {
                        if (instr->Bit(24) == 0) {
                            // multiply instructions
                            if (instr->Bit(23) == 0) {
                                if (instr->Bit(21) == 0) {
                                    // The MUL instruction description (A 4.1.33) refers to Rd as being
                                    // the destination for the operation, but it confusingly uses the
                                    // Rn field to encode it.
                                    Format(instr, "mul'cond's 'rn, 'rm, 'rs");
                                } else {
                                    // The MLA instruction description (A 4.1.28) refers to the order
                                    // of registers as "Rd, Rm, Rs, Rn". But confusingly it uses the
                                    // Rn field to encode the Rd register and the Rd field to encode
                                    // the Rn register.
                                    Format(instr, "mla'cond's 'rn, 'rm, 'rs, 'rd");
                                }
                            } else {
                                // The signed/long multiply instructions use the terms RdHi and RdLo
                                // when referring to the target registers. They are mapped to the Rn
                                // and Rd fields as follows:
                                // RdLo == Rd field
                                // RdHi == Rn field
                                // The order of registers is: <RdLo>, <RdHi>, <Rm>, <Rs>
                                Format(instr, "'um'al'cond's 'rd, 'rn, 'rm, 'rs");
                            }
                        } else {
                            Unknown(instr);  // not used by V8
                        }
                    } else if ((instr->Bit(20) == 0) && ((instr->Bits(7, 4) & 0xd) == 0xd)) {
                        // ldrd, strd
                        switch (instr->PUField()) {
                            case 0: {
                                        if (instr->Bit(22) == 0)
                                            Format(instr, "'memop'cond's 'rd, ['rn], -'rm");
                                        else
                                            Format(instr, "'memop'cond's 'rd, ['rn], #-'off8");
                                        break;
                                    }
                            case 1: {
                                        if (instr->Bit(22) == 0)
                                            Format(instr, "'memop'cond's 'rd, ['rn], +'rm");
                                        else
                                            Format(instr, "'memop'cond's 'rd, ['rn], #+'off8");
                                        break;
                                    }
                            case 2: {
                                        if (instr->Bit(22) == 0)
                                            Format(instr, "'memop'cond's 'rd, ['rn, -'rm]'w");
                                        else
                                            Format(instr, "'memop'cond's 'rd, ['rn, #-'off8]'w");
                                        break;
                                    }
                            case 3: {
                                        if (instr->Bit(22) == 0)
                                            Format(instr, "'memop'cond's 'rd, ['rn, +'rm]'w");
                                        else
                                            Format(instr, "'memop'cond's 'rd, ['rn, #+'off8]'w");
                                        break;
                                    }
                            default: {
                                         // The PU field is a 2-bit field.
                                         UNREACHABLE();
                                         break;
                                     }
                        }
                    } else {
                        // extra load/store instructions
                        switch (instr->PUField()) {
                            case 0:
                                if (instr->Bit(22) == 0)
                                    Format(instr, "'memop'cond'sign'h 'rd, ['rn], -'rm");
                                else
                                    Format(instr, "'memop'cond'sign'h 'rd, ['rn], #-'off8");
                                break;
                            case 1:
                                if (instr->Bit(22) == 0)
                                    Format(instr, "'memop'cond'sign'h 'rd, ['rn], +'rm");
                                else
                                    Format(instr, "'memop'cond'sign'h 'rd, ['rn], #+'off8");
                                break;
                            case 2:
                                if (instr->Bit(22) == 0)
                                    Format(instr, "'memop'cond'sign'h 'rd, ['rn, -'rm]'w");
                                else
                                    Format(instr, "'memop'cond'sign'h 'rd, ['rn, #-'off8]'w");
                                break;
                            case 3:
                                if (instr->Bit(22) == 0)
                                    Format(instr, "'memop'cond'sign'h 'rd, ['rn, +'rm]'w");
                                else
                                    Format(instr, "'memop'cond'sign'h 'rd, ['rn, #+'off8]'w");
                                break;
                            default:
                                // The PU field is a 2-bit field.
                                UNREACHABLE();
                                break;
                        }
                        return;
                    }
                } else if ((type == 0) && instr->IsMiscType0()) {
                    if (instr->Bits(22, 21) == 1) {
                        switch (instr->Bits(7, 4)) {
                            case BX:
                                Format(instr, "bx'cond 'rm");
                                break;
                            case BLX:
                                Format(instr, "blx'cond 'rm");
                                break;
                            case BKPT:
                                Format(instr, "bkpt 'off0to3and8to19");
                                break;
                            default:
                                Unknown(instr);  // not used by V8
                                break;
                        }
                    } else if (instr->Bits(22, 21) == 3) {
                        switch (instr->Bits(7, 4)) {
                            case CLZ:
                                Format(instr, "clz'cond 'rd, 'rm");
                                break;
                            default:
                                Unknown(instr);  // not used by V8
                                break;
                        }
                    } else {
                        Unknown(instr);  // not used by V8
                    }
                } else {
                    switch (instr->OpcodeField()) {
                        case AND:
                            Format(instr, "and'cond's 'rd, 'rn, 'shift_op");
                            break;
                        case EOR:
                            Format(instr, "eor'cond's 'rd, 'rn, 'shift_op");
                            break;
                        case SUB:
                            Format(instr, "sub'cond's 'rd, 'rn, 'shift_op");
                            break;
                        case RSB:
                            Format(instr, "rsb'cond's 'rd, 'rn, 'shift_op");
                            break;
                        case ADD:
                            Format(instr, "add'cond's 'rd, 'rn, 'shift_op");
                            break;
                        case ADC:
                            Format(instr, "adc'cond's 'rd, 'rn, 'shift_op");
                            break;
                        case SBC:
                            Format(instr, "sbc'cond's 'rd, 'rn, 'shift_op");
                            break;
                        case RSC:
                            Format(instr, "rsc'cond's 'rd, 'rn, 'shift_op");
                            break;
                        case TST:
                            if (instr->HasS())
                                Format(instr, "tst'cond 'rn, 'shift_op");
                            else
                                Format(instr, "movw'cond 'mw");
                            break;
                        case TEQ:
                            if (instr->HasS()) {
                                Format(instr, "teq'cond 'rn, 'shift_op");
                            } else {
                                // Other instructions matching this pattern are handled in the
                                // miscellaneous instructions part above.
                                UNREACHABLE();
                                break;
                            }
                        case CMP:
                            if (instr->HasS())
                                Format(instr, "cmp'cond 'rn, 'shift_op");
                            else
                                Format(instr, "movt'cond 'mw");
                            break;
                        case CMN:
                            if (instr->HasS()) {
                                Format(instr, "cmn'cond 'rn, 'shift_op");
                            } else {
                                // Other instructions matching this pattern are handled in the
                                // miscellaneous instructions part above.
                                UNREACHABLE();
                            }
                            break;
                        case ORR:
                            Format(instr, "orr'cond's 'rd, 'rn, 'shift_op");
                            break;
                        case MOV:
                            Format(instr, "mov'cond's 'rd, 'shift_op");
                            break;
                        case BIC:
                            Format(instr, "bic'cond's 'rd, 'rn, 'shift_op");
                            break;
                        case MVN:
                            Format(instr, "mvn'cond's 'rd, 'shift_op");
                            break;
                        default:
                            // The Opcode field is a 4-bit field.
                            UNREACHABLE();
                            break;
                    }
                }
            }

            static void DecodeType2(Instr* instr) {
                switch (instr->PUField()) {
                    case 0:
                        if (instr->HasW())
                            Unknown(instr);  // not used in V8
                        Format(instr, "'memop'cond'b 'rd, ['rn], #-'off12");
                        break;
                    case 1:
                        if (instr->HasW())
                            Unknown(instr);  // not used in V8
                        Format(instr, "'memop'cond'b 'rd, ['rn], #+'off12");
                        break;
                    case 2:
                        Format(instr, "'memop'cond'b 'rd, ['rn, #-'off12]'w");
                        break;
                    case 3:
                        Format(instr, "'memop'cond'b 'rd, ['rn, #+'off12]'w");
                        break;
                    default:
                        // The PU field is a 2-bit field.
                        UNREACHABLE();
                        break;
                }
            }


            static void DecodeType3(Instr* instr) {
                switch (instr->PUField()) {
                    case 0:
                        assert(!instr->HasW());
                        Format(instr, "'memop'cond'b 'rd, ['rn], -'shift_rm");
                        break;
                    case 1:
                        if (instr->HasW()) {
                            assert(instr->Bits(5, 4) == 0x1);
                            if (instr->Bit(22) == 0x1)
                                Format(instr, "usat 'rd, #'imm05@16, 'rm'shift_sat");
                            else
                                UNREACHABLE();  // SSAT.
                        } else {
                            Format(instr, "'memop'cond'b 'rd, ['rn], +'shift_rm");
                        }
                        break;
                    case 2:
                        Format(instr, "'memop'cond'b 'rd, ['rn, -'shift_rm]'w");
                        break;
                    case 3:
                        if (instr->HasW() && instr->Bits(6, 4) == 0x5) {
                            uint32_t widthminus1 = static_cast<uint32_t>(instr->Bits(20, 16));
                            uint32_t lsbit = static_cast<uint32_t>(instr->Bits(11, 7));
                            uint32_t msbit = widthminus1 + lsbit;
                            if (msbit <= 31) {
                                if (instr->Bit(22))
                                    Format(instr, "ubfx'cond 'rd, 'rm, 'f");
                                else
                                    Format(instr, "sbfx'cond 'rd, 'rm, 'f");
                            } else {
                                UNREACHABLE();
                            }
                        } else if (!instr->HasW() && instr->Bits(6, 4) == 0x1) {
                            uint32_t lsbit = static_cast<uint32_t>(instr->Bits(11, 7));
                            uint32_t msbit = static_cast<uint32_t>(instr->Bits(20, 16));
                            if (msbit >= lsbit) {
                                if (instr->RmField() == 15)
                                    Format(instr, "bfc'cond 'rd, 'f");
                                else
                                    Format(instr, "bfi'cond 'rd, 'rm, 'f");
                            } else {
                                UNREACHABLE();
                            }
                        } else {
                            Format(instr, "'memop'cond'b 'rd, ['rn, +'shift_rm]'w");
                        }
                        break;
                    default:
                        // The PU field is a 2-bit field.
                        UNREACHABLE();
                        break;
                }
            }


            static void DecodeType4(Instr* instr) {
                if (instr->Bit(22) != 0){
                    Format(instr, ".data");
                    return ;
                }
                assert(instr->Bit(22) == 0);  // Privileged mode currently not supported.
                if (instr->HasL())
                    Format(instr, "ldm'cond'pu 'rn'w, 'rlist");
                else
                    Format(instr, "stm'cond'pu 'rn'w, 'rlist");
            }


            static void DecodeType5(Instr* instr) {
                Format(instr, "b'l'cond 'target");
            }


            static void DecodeType6(Instr* instr) {
                DecodeType6CoprocessorIns(instr);
            }


            static void DecodeType7(Instr* instr) {
                if (instr->Bit(24) == 1)
                    Format(instr, "swi'cond 'swi");
                else
                    DecodeTypeVFP(instr);
            }

            // void Decoder::DecodeTypeVFP(Instr* instr)
            // vmov: Sn = Rt
            // vmov: Rt = Sn
            // vcvt: Dd = Sm
            // vcvt: Sd = Dm
            // Dd = vadd(Dn, Dm)
            // Dd = vsub(Dn, Dm)
            // Dd = vmul(Dn, Dm)
            // Dd = vdiv(Dn, Dm)
            // vcmp(Dd, Dm)
            // vmrs
            // Dd = vsqrt(Dm)
            static void DecodeTypeVFP(Instr* instr) {
                assert((instr->TypeField() == 7) && (instr->Bit(24) == 0x0) );

                if (instr->Bits(11, 9) != 0x5) {
                    if (instr->Bits(27,24) == 0xe && instr->Bit(20) == 0x0 && instr->Bit(4) == 1)
                    {
                        Format(instr, "mcr but it should be data");
                        return;
                    }
                    if (instr->Bits(27,24) == 0xe && instr->Bit(20) == 0x1 && instr->Bit(4) == 1)
                    {
                        Format(instr, "mrc but it should be data");
                        return;
                    }
                }


                assert(instr->Bits(11, 9) == 0x5);

                if (instr->Bit(4) == 0) {
                    if (instr->Opc1Field() == 0x7) {
                        // Other data processing instructions
                        if (instr->Opc2Field() == 0x0 && instr->Opc3Field() == 0x1) {
                            // vmov register to register.
                            if (instr->SzField() == 0x1)
                                Format(instr, "vmov.f64'cond 'Dd, 'Dm");
                            else
                                Format(instr, "vmov.f32'cond 'Sd, 'Sm");
                        } else if (instr->Opc2Field() == 0x7 && instr->Opc3Field() == 0x3) {
                            DecodeVCVTBetweenDoubleAndSingle(instr);
                        } else if (instr->Opc2Field() == 0x8 && (instr->Opc3Field() & 0x1)) {
                            DecodeVCVTBetweenFloatingPointAndInteger(instr);
                        } else if ((instr->Opc2Field() >> 1) == 0x6 &&
                                (instr->Opc3Field() & 0x1)) {
                            DecodeVCVTBetweenFloatingPointAndInteger(instr);
                        } else if ((instr->Opc2Field() == 0x4 || instr->Opc2Field() == 0x5) && (instr->Opc3Field() & 0x1)) {
                            DecodeVCMP(instr);
                        } else if (instr->Opc2Field() == 0x1 && instr->Opc3Field() == 0x3) {
                            Format(instr, "vsqrt.f64'cond 'Dd, 'Dm");
                        } else if (instr->Opc3Field() == 0x0) {
                            if (instr->SzField() == 0x1)
                                Format(instr, "vmov.f64'cond 'Dd, 'd");
                            else
                                Unknown(instr);  // Not used by V8.
                        } else {
                            Unknown(instr);  // Not used by V8.
                        }
                    } else if (instr->Opc1Field() == 0x3) {
                        if (instr->SzField() == 0x1) {
                            if (instr->Opc3Field() & 0x1)
                                Format(instr, "vsub.f64'cond 'Dd, 'Dn, 'Dm");
                            else
                                Format(instr, "vadd.f64'cond 'Dd, 'Dn, 'Dm");
                        } else {
                            Unknown(instr);  // Not used by V8.
                        }
                    } else if (instr->Opc1Field() == 0x2 && !(instr->Opc3Field() & 0x1)) {
                        if (instr->SzField() == 0x1)
                            Format(instr, "vmul.f64'cond 'Dd, 'Dn, 'Dm");
                        else
                            Unknown(instr);  // Not used by V8.
                    } else if (instr->Opc1Field() == 0x4 && !(instr->Opc3Field() & 0x1)) {
                        if (instr->SzField() == 0x1)
                            Format(instr, "vdiv.f64'cond 'Dd, 'Dn, 'Dm");
                        else
                            Unknown(instr);  // Not used by V8.
                    } else {
                        Unknown(instr);  // Not used by V8.
                    }
                } else {
                    if (instr->VCField() == 0x0 && instr->VAField() == 0x0) {
                        DecodeVMOVBetweenCoreAndSinglePrecisionRegisters(instr);
                    } else if (instr->VLField() == 0x1 &&
                            instr->VCField() == 0x0 &&
                            instr->VAField() == 0x7 &&
                            instr->Bits(19, 16) == 0x1) {
                        if (instr->Bits(15, 12) == 0xF)
                            Format(instr, "vmrs'cond APSR, FPSCR");
                        else
                            Unknown(instr);  // Not used by V8.
                    } else {
                        Unknown(instr);  // Not used by V8.
                    }
                }
            }


            static void DecodeVMOVBetweenCoreAndSinglePrecisionRegisters(Instr* instr) {
                assert((instr->Bit(4) == 1) && (instr->VCField() == 0x0) &&
                        (instr->VAField() == 0x0));

                bool to_arm_register = (instr->VLField() == 0x1);

                if (to_arm_register)
                    Format(instr, "vmov'cond 'rt, 'Sn");
                else
                    Format(instr, "vmov'cond 'Sn, 'rt");
            }


            static void DecodeVCMP(Instr* instr) {
                assert((instr->Bit(4) == 0) && (instr->Opc1Field() == 0x7));
                assert(((instr->Opc2Field() == 0x4) || (instr->Opc2Field() == 0x5)) &&
                        (instr->Opc3Field() & 0x1));

                // Comparison.
                bool dp_operation = (instr->SzField() == 1);
                bool raise_exception_for_qnan = (instr->Bit(7) == 0x1);

                if (dp_operation && !raise_exception_for_qnan) {
                    if (instr->Opc2Field() == 0x4)
                        Format(instr, "vcmp.f64'cond 'Dd, 'Dm");
                    else if (instr->Opc2Field() == 0x5)
                        Format(instr, "vcmp.f64'cond 'Dd, #0.0");
                    else
                        Unknown(instr);  // invalid
                } else {
                    Unknown(instr);  // Not used by V8.
                }
            }


            static void DecodeVCVTBetweenDoubleAndSingle(Instr* instr) {
                assert((instr->Bit(4) == 0) && (instr->Opc1Field() == 0x7));
                assert((instr->Opc2Field() == 0x7) && (instr->Opc3Field() == 0x3));

                bool double_to_single = (instr->SzField() == 1);

                if (double_to_single)
                    Format(instr, "vcvt.f32.f64'cond 'Sd, 'Dm");
                else
                    Format(instr, "vcvt.f64.f32'cond 'Dd, 'Sm");
            }


            static void DecodeVCVTBetweenFloatingPointAndInteger(Instr* instr) {
                assert((instr->Bit(4) == 0) && (instr->Opc1Field() == 0x7));
                assert(((instr->Opc2Field() == 0x8) && (instr->Opc3Field() & 0x1)) ||
                        (((instr->Opc2Field() >> 1) == 0x6) && (instr->Opc3Field() & 0x1)));

                bool to_integer = (instr->Bit(18) == 1);
                bool dp_operation = (instr->SzField() == 1);
                if (to_integer) {
                    bool unsigned_integer = (instr->Bit(16) == 0);

                    if (dp_operation) {
                        if (unsigned_integer)
                            Format(instr, "vcvt.u32.f64'cond 'Sd, 'Dm");
                        else
                            Format(instr, "vcvt.s32.f64'cond 'Sd, 'Dm");
                    } else {
                        if (unsigned_integer)
                            Format(instr, "vcvt.u32.f32'cond 'Sd, 'Sm");
                        else
                            Format(instr, "vcvt.s32.f32'cond 'Sd, 'Sm");
                    }
                } else {
                    bool unsigned_integer = (instr->Bit(7) == 0);

                    if (dp_operation) {
                        if (unsigned_integer)
                            Format(instr, "vcvt.f64.u32'cond 'Dd, 'Sm");
                        else
                            Format(instr, "vcvt.f64.s32'cond 'Dd, 'Sm");
                    } else {
                        if (unsigned_integer)
                            Format(instr, "vcvt.f32.u32'cond 'Sd, 'Sm");
                        else
                            Format(instr, "vcvt.f32.s32'cond 'Sd, 'Sm");
                    }
                }
            }


            // Decode Type 6 coprocessor instructions.
            // Dm = vmov(Rt, Rt2)
            // <Rt, Rt2> = vmov(Dm)
            // Ddst = MEM(Rbase + 4*offset).
            // MEM(Rbase + 4*offset) = Dsrc.
            static void DecodeType6CoprocessorIns(Instr* instr) {
                assert((instr->TypeField() == 6));

                if (instr->CoprocessorField() == 0xA) {
                    switch (instr->OpcodeField()) {
                        case 0x8:
                        case 0xA:
                            if (instr->HasL())
                                Format(instr, "vldr'cond 'Sd, ['rn - 4*'imm08@00]");
                            else
                                Format(instr, "vstr'cond 'Sd, ['rn - 4*'imm08@00]");
                            break;
                        case 0xC:
                        case 0xE:
                            if (instr->HasL())
                                Format(instr, "vldr'cond 'Sd, ['rn + 4*'imm08@00]");
                            else
                                Format(instr, "vstr'cond 'Sd, ['rn + 4*'imm08@00]");
                            break;
                        default:
                            Unknown(instr);  // Not used by V8.
                            break;
                    }
                } else if (instr->CoprocessorField() == 0xB) {
                    switch (instr->OpcodeField()) {
                        case 0x2:
                            // Load and store double to two GP registers
                            if (instr->Bits(7, 4) != 0x1)
                                Unknown(instr);  // Not used by V8.
                            else if (instr->HasL())
                                Format(instr, "vmov'cond 'rt, 'rn, 'Dm");
                            else
                                Format(instr, "vmov'cond 'Dm, 'rt, 'rn");
                            break;
                        case 0x8:
                            if (instr->HasL())
                                Format(instr, "vldr'cond 'Dd, ['rn - 4*'imm08@00]");
                            else
                                Format(instr, "vstr'cond 'Dd, ['rn - 4*'imm08@00]");
                            break;
                        case 0xC:
                            if (instr->HasL())
                                Format(instr, "vldr'cond 'Dd, ['rn + 4*'imm08@00]");
                            else
                                Format(instr, "vstr'cond 'Dd, ['rn + 4*'imm08@00]");
                            break;
                        default:
                            Unknown(instr);  // Not used by V8.
                            break;
                    }
                } else {
                    UNIMPLEMENTED();  // Not used by V8.
                }
            }

            // Support for assertions in the Decoder formatting functions.
#define STRING_STARTS_WITH(string, compare_string) \
            (strncmp(string, compare_string, strlen(compare_string)) == 0)
            // Append the ch to the output buffer.
            static void PrintChar(const char ch) {
                out_buffer_[out_buffer_pos_++] = ch;
            }


            // Append the str to the output buffer.
            static void Print(const char* str) {
                char cur = *str++;
                while (cur != '\0' && (out_buffer_pos_ < buffer_length_)) {
                    PrintChar(cur);
                    cur = *str++;
                }
                out_buffer_[out_buffer_pos_] = 0;
            }




            // Print the condition guarding the instruction.
            static void PrintCondition(Instr* instr) {
                Print(cond_names[instr->ConditionField()]);
            }


            // Print the register name according to the active name converter.
            static void PrintRegister(int reg) {
                Print(Registers::Name(reg));
            }

            // Print the VFP S register name according to the active name converter.
            static void PrintSRegister(int reg) {
                Print(VFPRegisters::Name(reg, false));
            }

            // Print the  VFP D register name according to the active name converter.
            static void PrintDRegister(int reg) {
                Print(VFPRegisters::Name(reg, true));
            }


            // Print the register shift operands for the instruction. Generally used for
            // data processing instructions.
            static void PrintShiftRm(Instr* instr) {
                Shift shift = instr->ShiftField();
                int shift_amount = instr->ShiftAmountField();
                int rm = instr->RmField();

                PrintRegister(rm);

                if (instr->RegShiftField() == 0 && shift == LSL && shift_amount == 0) {
                    // Special case for using rm only.
                    return;
                }
                if (instr->RegShiftField() == 0) {
                    // by immediate
                    if ((shift == ROR) && (shift_amount == 0)) {
                        Print(", RRX");
                        return;
                    } else if ((shift == LSR || shift == ASR) && shift_amount == 0) {
                        shift_amount = 32;
                    }
                    out_buffer_pos_ += sprintf(out_buffer_ + out_buffer_pos_,
                            ", %s #%d",
                            shift_names[shift], shift_amount);
                } else {
                    // by register
                    int rs = instr->RsField();
                    out_buffer_pos_ += sprintf(out_buffer_ + out_buffer_pos_,
                            ", %s ", shift_names[shift]);
                    PrintRegister(rs);
                }
            }


            // Print the immediate operand for the instruction. Generally used for data
            // processing instructions.
            static void PrintShiftImm(Instr* instr) {
                int rotate = instr->RotateField() * 2;
                int immed8 = instr->Immed8Field();
                int imm = (immed8 >> rotate) | (immed8 << (32 - rotate));
                out_buffer_pos_ += sprintf(out_buffer_ + out_buffer_pos_,
                        "#%d", imm);
            }


            // Print the optional shift and immediate used by saturating instructions.
            static void PrintShiftSat(Instr* instr) {
                int shift = instr->Bits(11, 7);
                if (shift > 0) {
                    out_buffer_pos_ += sprintf(out_buffer_ + out_buffer_pos_,
                            ", %s #%d",
                            shift_names[instr->Bit(6) * 2],
                            instr->Bits(11, 7));
                }
            }


            // Print PU formatting to reduce complexity of FormatOption.
            static void PrintPU(Instr* instr) {
                switch (instr->PUField()) {
                    case 0:
                        Print("da");
                        break;
                    case 1:
                        Print("ia");
                        break;
                    case 2:
                        Print("db");
                        break;
                    case 3:
                        Print("ib");
                        break;
                    default:
                        UNREACHABLE();
                        break;
                }
            }


            // Print SoftwareInterrupt codes. Factoring this out reduces the complexity of
            // the FormatOption method.
            static void PrintSoftwareInterrupt(SoftwareInterruptCodes swi) {
                switch (swi) {
                    case call_rt_redirected:
                        Print("call_rt_redirected");
                        return;
                    case break_point:
                        Print("break_point");
                        return;
                    default:
                        out_buffer_pos_ += sprintf(out_buffer_ + out_buffer_pos_,
                                "%d",
                                swi);
                        return;
                }
            }


            // Handle all register based formatting in this function to reduce the
            // complexity of FormatOption.
            static int FormatRegister(Instr* instr, const char* format) {
                assert(format[0] == 'r');
                if (format[1] == 'n') {  // 'rn: Rn register
                    int reg = instr->RnField();
                    PrintRegister(reg);
                    return 2;
                } else if (format[1] == 'd') {  // 'rd: Rd register
                    int reg = instr->RdField();
                    PrintRegister(reg);
                    return 2;
                } else if (format[1] == 's') {  // 'rs: Rs register
                    int reg = instr->RsField();
                    PrintRegister(reg);
                    return 2;
                } else if (format[1] == 'm') {  // 'rm: Rm register
                    int reg = instr->RmField();
                    PrintRegister(reg);
                    return 2;
                } else if (format[1] == 't') {  // 'rt: Rt register
                    int reg = instr->RtField();
                    PrintRegister(reg);
                    return 2;
                } else if (format[1] == 'l') {
                    // 'rlist: register list for load and store multiple instructions
                    assert(STRING_STARTS_WITH(format, "rlist"));
                    int rlist = instr->RlistField();
                    int reg = 0;
                    Print("{");
                    // Print register list in ascending order, by scanning the bit mask.
                    while (rlist != 0) {
                        if ((rlist & 1) != 0) {
                            PrintRegister(reg);
                            if ((rlist >> 1) != 0)
                                Print(", ");
                        }
                        reg++;
                        rlist >>= 1;
                    }
                    Print("}");
                    return 5;
                }
                UNREACHABLE();
                return -1;
            }


            // Handle all VFP register based formatting in this function to reduce the
            // complexity of FormatOption.
            static int FormatVFPRegister(Instr* instr, const char* format) {
                assert((format[0] == 'S') || (format[0] == 'D'));

                if (format[1] == 'n') {
                    int reg = instr->VnField();
                    if (format[0] == 'S')
                        PrintSRegister(((reg << 1) | instr->NField()));
                    if (format[0] == 'D')
                        PrintDRegister(reg);
                    return 2;
                } else if (format[1] == 'm') {
                    int reg = instr->VmField();
                    if (format[0] == 'S')
                        PrintSRegister(((reg << 1) | instr->MField()));
                    if (format[0] == 'D')
                        PrintDRegister(reg);
                    return 2;
                } else if (format[1] == 'd') {
                    int reg = instr->VdField();
                    if (format[0] == 'S')
                        PrintSRegister(((reg << 1) | instr->DField()));
                    if (format[0] == 'D')
                        PrintDRegister(reg);
                    return 2;
                }

                UNREACHABLE();
                return -1;
            }


            static int FormatVFPinstruction(Instr* instr, const char* format) {
                Print(format);
                return 0;
            }


            // Print the movw or movt instruction.
            static void PrintMovwMovt(Instr* instr) {
                int imm = instr->ImmedMovwMovtField();
                int rd = instr->RdField();
                PrintRegister(rd);
                out_buffer_pos_ += sprintf(out_buffer_ + out_buffer_pos_,
                        ", #%d", imm);
            }


            // FormatOption takes a formatting string and interprets it based on
            // the current instructions. The format string points to the first
            // character of the option string (the option escape has already been
            // consumed by the caller.)  FormatOption returns the number of
            // characters that were consumed from the formatting string.
            static int FormatOption(Instr* instr, const char* format) {
                switch (format[0]) {
                    case 'a':  // 'a: accumulate multiplies
                        if (instr->Bit(21) == 0)
                            Print("ul");
                        else
                            Print("la");
                        return 1;
                    case 'b':  // 'b: byte loads or stores
                        if (instr->HasB())
                            Print("b");
                        return 1;
                    case 'c':  // 'cond: conditional execution
                        assert(STRING_STARTS_WITH(format, "cond"));
                        PrintCondition(instr);
                        return 4;
                    case 'd': {  // 'd: vmov double immediate.
                                  double d = instr->DoubleImmedVmov();
                                  out_buffer_pos_ += sprintf(out_buffer_ + out_buffer_pos_, "#%g", d);
                                  return 1;
                              }
                    case 'f': {  // 'f: bitfield instructions - v7 and above.
                                  uint32_t lsbit = instr->Bits(11, 7);
                                  uint32_t width = instr->Bits(20, 16) + 1;
                                  if (instr->Bit(21) == 0) {
                                      // BFC/BFI:
                                      // Bits 20-16 represent most-significant bit. Covert to width.
                                      width -= lsbit;
                                      assert(width > 0);
                                  }
                                  assert((width + lsbit) <= 32);
                                  out_buffer_pos_ += sprintf(out_buffer_ + out_buffer_pos_,
                                          "#%d, #%d", lsbit, width);
                                  return 1;
                              }
                    case 'h':  // 'h: halfword operation for extra loads and stores
                              if (instr->HasH())
                                  Print("h");
                              else
                                  Print("b");
                              return 1;
                    case 'i': {  // 'i: immediate value from adjacent bits.
                                  // Expects tokens in the form imm%02d@%02d, ie. imm05@07, imm10@16
                                  int width = (format[3] - '0') * 10 + (format[4] - '0');
                                  int lsb   = (format[6] - '0') * 10 + (format[7] - '0');

                                  assert((width >= 1) && (width <= 32));
                                  assert((lsb >= 0) && (lsb <= 31));
                                  assert((width + lsb) <= 32);

                                  out_buffer_pos_ += sprintf(out_buffer_ + out_buffer_pos_,
                                          "%d",
                                          instr->Bits(width + lsb - 1, lsb));
                                  return 8;
                              }
                    case 'l':  // 'l: branch and link
                              if (instr->HasLink())
                                  Print("l");
                              return 1;
                    case 'm': {
                                  if (format[1] == 'w') {
                                      // 'mw: movt/movw instructions.
                                      PrintMovwMovt(instr);
                                      return 2;
                                  }
                                  if (format[1] == 'e') {  // 'memop: load/store instructions.
                                      assert(STRING_STARTS_WITH(format, "memop"));
                                      if (instr->HasL()) {
                                          Print("ldr");
                                      } else if ((instr->Bits(27, 25) == 0) && (instr->Bit(20) == 0)) {
                                          if (instr->Bits(7, 4) == 0xf) {
                                              Print("strd");
                                          } else {
                                              Print("ldrd");
                                          }
                                      } else {
                                          Print("str");
                                      }
                                      return 5;
                                  }
                                  // 'msg: for simulator break instructions
                                  assert(STRING_STARTS_WITH(format, "msg"));
                                  //byte* str =
                                  //	reinterpret_cast<byte*>(instr->InstructionBits() & 0x0fffffff);
                                  out_buffer_pos_ += sprintf(out_buffer_ + out_buffer_pos_,
                                          "%s", "");//converter_.NameInCode(str));
                                  return 3;
                              }
                    case 'o': {
                                  if ((format[3] == '1') && (format[4] == '2')) {
                                      // 'off12: 12-bit offset for load and store instructions
                                      assert(STRING_STARTS_WITH(format, "off12"));
                                      out_buffer_pos_ += sprintf(out_buffer_ + out_buffer_pos_,
                                              "%d", instr->Offset12Field());
                                      return 5;
                                  } else if (format[3] == '0') {
                                      // 'off0to3and8to19 16-bit immediate encoded in bits 19-8 and 3-0.
                                      assert(STRING_STARTS_WITH(format, "off0to3and8to19"));
                                      out_buffer_pos_ += sprintf(out_buffer_ + out_buffer_pos_,
                                              "%d",
                                              (instr->Bits(19, 8) << 4) +
                                              instr->Bits(3, 0));
                                      return 15;
                                  }
                                  // 'off8: 8-bit offset for extra load and store instructions
                                  assert(STRING_STARTS_WITH(format, "off8"));
                                  int offs8 = (instr->ImmedHField() << 4) | instr->ImmedLField();
                                  out_buffer_pos_ += sprintf(out_buffer_ + out_buffer_pos_,
                                          "%d", offs8);
                                  return 4;
                              }
                    case 'p':  // 'pu: P and U bits for load and store instructions
                              assert(STRING_STARTS_WITH(format, "pu"));
                              PrintPU(instr);
                              return 2;
                    case 'r':
                              return FormatRegister(instr, format);
                    case 's':
                              if (format[1] == 'h') {  // 'shift_op or 'shift_rm or 'shift_sat.
                                  if (format[6] == 'o') {  // 'shift_op
                                      assert(STRING_STARTS_WITH(format, "shift_op"));
                                      if (instr->TypeField() == 0) {
                                          PrintShiftRm(instr);
                                      } else {
                                          assert(instr->TypeField() == 1);
                                          PrintShiftImm(instr);
                                      }
                                      return 8;
                                  } else if (format[6] == 's') {  // 'shift_sat.
                                      assert(STRING_STARTS_WITH(format, "shift_sat"));
                                      PrintShiftSat(instr);
                                      return 9;
                                  } else {  // 'shift_rm
                                      assert(STRING_STARTS_WITH(format, "shift_rm"));
                                      PrintShiftRm(instr);
                                      return 8;
                                  }
                              } else if (format[1] == 'w') {  // 'swi
                                  assert(STRING_STARTS_WITH(format, "swi"));
                                  PrintSoftwareInterrupt(instr->SwiField());
                                  return 3;
                              } else if (format[1] == 'i') {  // 'sign: signed extra loads and stores
                                  assert(STRING_STARTS_WITH(format, "sign"));
                                  if (instr->HasSign())
                                      Print("s");
                                  return 4;
                              }
                              // 's: S field of data processing instructions
                              if (instr->HasS())
                                  Print("s");
                              return 1;
                    case 't': {  // 'target: target of branch instructions
                                  assert(STRING_STARTS_WITH(format, "target"));
                                  int off = (instr->SImmed24Field() << 2) + 8;
                                  out_buffer_pos_ += sprintf(
                                          out_buffer_ + out_buffer_pos_,
                                          "%+d -> %p",
                                          off,
                                          reinterpret_cast<byte*>(instr) + off);
                                  return 6;
                              }
                    case 'u':  // 'u: signed or unsigned multiplies
                              // The manual gets the meaning of bit 22 backwards in the multiply
                              // instruction overview on page A3.16.2.  The instructions that
                              // exist in u and s variants are the following:
                              // smull A4.1.87
                              // umull A4.1.129
                              // umlal A4.1.128
                              // smlal A4.1.76
                              // For these 0 means u and 1 means s.  As can be seen on their individual
                              // pages.  The other 18 mul instructions have the bit set or unset in
                              // arbitrary ways that are unrelated to the signedness of the instruction.
                              // None of these 18 instructions exist in both a 'u' and an 's' variant.

                              if (instr->Bit(22) == 0)
                                  Print("u");
                              else
                                  Print("s");
                              return 1;
                    case 'v':
                              return FormatVFPinstruction(instr, format);
                    case 'S':
                    case 'D':
                              return FormatVFPRegister(instr, format);
                    case 'w':  // 'w: W field of load and store instructions
                              if (instr->HasW())
                                  Print("!");
                              return 1;
                    default:
                              UNREACHABLE();
                              break;
                }
                UNREACHABLE();
                return -1;
            }


            // Format takes a formatting string for a whole instruction and prints it into
            // the output buffer. All escaped options are handed to FormatOption to be
            // parsed further.
            static void Format(Instr* instr, const char* format) {
                char cur = *format++;
                out_buffer_pos_ = 0;
                while (cur != 0 && out_buffer_pos_ < buffer_length_) {
                    if (cur == '\'') // Single quote is used as the formatting escape.
                        format += FormatOption(instr, format);
                    else
                        out_buffer_[out_buffer_pos_++] = cur;
                    cur = *format++;
                }
                out_buffer_[out_buffer_pos_]  = '\0';
                printf("%s", out_buffer_);

            }
    }; //class Disassembler


    }; // namespace DISASM

#endif
#endif
