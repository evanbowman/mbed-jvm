#include "classfile.hpp"
#include "class.hpp"
#include "endian.hpp"
#include "object.hpp"
#include <iostream>
#include <map>
#include <string.h>
#include <vector>



namespace java {
namespace jvm {



std::vector<void*> __operand_stack;
std::vector<void*> __locals;


void store_local(int index, void* value)
{
    __locals[(__locals.size() - 1) - index] = value;
}


void* load_local(int index)
{
    return __locals[(__locals.size() - 1) - index];
}


void alloc_locals(int count)
{
    for (int i = 0; i < count; ++i) {
        __locals.push_back(nullptr);
    }
}


void free_locals(int count)
{
    for (int i = 0; i < count; ++i) {
        __locals.pop_back();
    }
}


void push_operand(void* value)
{
    __operand_stack.push_back(value);
}


void push_operand(float* value)
{
    push_operand((void*)(intptr_t)*(int*)value);
}


void* load_operand(int offset)
{
    return __operand_stack[(__operand_stack.size() - 1) - offset];
}


float __load_operand_f_impl(void** val)
{
    return *(float*)val;
}


float load_operand_f(int offset)
{
    auto val = load_operand(offset);
    return __load_operand_f_impl(&val);
}


int load_operand_i(int offset)
{
    return (int)(intptr_t)load_operand(offset);
}


void pop_operand()
{
    __operand_stack.pop_back();
}



struct Bytecode {
    enum : u8 {
        nop           = 0x00,
        pop           = 0x57,
        ldc           = 0x12,
        new_inst      = 0xbb,
        dup           = 0x59,
        bipush        = 0x10,
        aload         = 0x19,
        aload_0       = 0x2a,
        aload_1       = 0x2b,
        aload_2       = 0x2c,
        aload_3       = 0x2d,
        astore        = 0x3a,
        astore_0      = 0x4b,
        astore_1      = 0x4c,
        astore_2      = 0x4d,
        astore_3      = 0x4e,
        areturn       = 0xb0,
        iconst_0      = 0x03,
        iconst_1      = 0x04,
        iconst_2      = 0x05,
        iconst_3      = 0x06,
        iconst_4      = 0x07,
        iconst_5      = 0x08,
        istore        = 0x36,
        istore_0      = 0x3b,
        istore_1      = 0x3c,
        istore_2      = 0x3d,
        istore_3      = 0x3e,
        iload         = 0x15,
        iload_0       = 0x1a,
        iload_1       = 0x1b,
        iload_2       = 0x1c,
        iload_3       = 0x1d,
        iadd          = 0x60,
        isub          = 0x64,
        idiv          = 0x6c,
        i2s           = 0x93,
        iinc          = 0x84,
        if_acmpeq     = 0xa5,
        if_acmpne     = 0xa6,
        if_icmpeq     = 0x9f,
        if_icmpne     = 0xa0,
        if_icmplt     = 0xa1,
        if_icmpge     = 0xa2,
        if_icmpgt     = 0xa3,
        if_icmple     = 0xa4,
        if_eq         = 0x99,
        if_ne         = 0x9a,
        if_lt         = 0x9b,
        if_ge         = 0x9c,
        if_gt         = 0x9d,
        if_le         = 0x9e,
        if_nonnull    = 0xc7,
        if_null       = 0xc6,
        fconst_0      = 0x0b,
        fconst_1      = 0x0c,
        fconst_2      = 0x0d,
        fadd          = 0x62,
        fdiv          = 0x6e,
        fmul          = 0x6a,
        getfield      = 0xb4,
        putfield      = 0xb5,
        __goto        = 0xa7,
        __goto_w      = 0xc8,
        invokestatic  = 0xb8,
        invokevirtual = 0xb6,
        invokespecial = 0xb7,
        vreturn       = 0xb1,
    };
};


struct ClassTableEntry {
    Slice name_;
    Class* class_;
};



static std::vector<ClassTableEntry> class_table;



void register_class(Slice name, Class* clz)
{
    class_table.push_back({name, clz});
}



void execute_bytecode(Class* clz, const u8* bytecode);



void invoke_method(Class* clz,
                   Object* self,
                   const ClassFile::MethodInfo* method)
{
    for (int i = 0; i < method->attributes_count_.get(); ++i) {
        auto attr =
            (ClassFile::AttributeInfo*)
            ((const char*)method + sizeof(ClassFile::MethodInfo));

        if (clz->constants_->load_string(attr->attribute_name_index_.get()) ==
            Slice::from_c_str("Code")) {

            auto bytecode =
                ((const u8*)attr)
                + sizeof(ClassFile::AttributeCode);

            const auto local_count =
                std::max(((ClassFile::AttributeCode*)attr)->max_locals_.get(),
                         (u16)4); // Why a min of four? istore_0-3, so there
                                  // must be at least four slots.

            alloc_locals(local_count);
            store_local(0, self);

            execute_bytecode(clz, bytecode);

            free_locals(local_count);
        }
    }
}



Class* load_class(Class* current_module, u16 class_index)
{
    auto c_clz = (const ClassFile::ConstantClass*)
        current_module->constants_->load(class_index);

    auto cname = current_module->constants_->load_string(c_clz->name_index_.get());

    for (auto& entry : class_table) {
        if (entry.name_ == cname) {
            return entry.class_;
        }
    }

    return nullptr;
}



const ClassFile::MethodInfo* lookup_method(Class* clz,
                                           Slice lhs_name,
                                           Slice lhs_type)
{
    if (clz->methods_) {
        for (int i = 0; i < clz->method_count_; ++i) {
            u16 name_index = clz->methods_[i]->name_index_.get();
            u16 type_index = clz->methods_[i]->descriptor_index_.get();

            auto rhs_name = clz->constants_->load_string(name_index);
            auto rhs_type = clz->constants_->load_string(type_index);

            if (lhs_type == rhs_type and lhs_name == rhs_name) {
                return clz->methods_[i];
            }
        }
    }

    return nullptr;
}



void dispatch_method(Class* clz, Object* self, u16 method_index)
{
    auto ref = (const ClassFile::ConstantRef*)clz->constants_->load(method_index);

    Class* t_clz = load_class(clz, ref->class_index_.get());
    if (t_clz == nullptr) {
        printf("failed to load class %d, TODO: raise error...\n",
               ref->class_index_.get());
        while (true) ;
    }

    auto nt = (const ClassFile::ConstantNameAndType*)
        clz->constants_->load(ref->name_and_type_index_.get());

    auto lhs_name = clz->constants_->load_string(nt->name_index_.get());
    auto lhs_type = clz->constants_->load_string(nt->descriptor_index_.get());

    if (auto mtd = lookup_method(t_clz, lhs_name, lhs_type)) {
        invoke_method(t_clz, self, mtd);
    } else {
        // TODO: raise error
        puts("method lookup failed!");
        while (true);
    }
}



void invoke_special(Class* clz, u16 method_index)
{
    puts("invoke_special");

    auto self = (Object*)load_operand(0);
    pop_operand();

    dispatch_method(clz, self, method_index);
}



void* malloc(size_t size)
{
    static size_t total = 0;
    total += size;
    printf("vm malloc %zu (total %zu)\n", size, total);
    return ::malloc(size);
}



Object* make_instance(Class* clz, u16 class_constant)
{
    auto t_clz = load_class(clz, class_constant);

    if (t_clz) {
        auto sub = (SubstitutionField*)clz->constants_->load(clz->cpool_highest_field_);

        printf("highest field offset: %d\n", sub->offset_);
        printf("instance size %ld\n", sizeof(Object) + sub->offset_ + (1 << sub->size_));

        auto mem = (Object*)jvm::malloc(sizeof(Object) + sub->offset_ + (1 << sub->size_));
        new (mem) Object();
        mem->class_ = t_clz;
        return mem;
    }

    // TODO: fatal error...
    puts("warning! failed to alloc class!");
    return nullptr;
}



void execute_bytecode(Class* clz, const u8* bytecode)
{
    u32 pc = 0;

    while (true) {
        switch (bytecode[pc]) {
        case Bytecode::nop:
            ++pc;
            break;

        case Bytecode::pop:
            pop_operand();
            ++pc;
            break;

        case Bytecode::ldc: {
            auto c = clz->constants_->load(bytecode[pc + 1]);
            switch (c->tag_) {
            case ClassFile::ConstantType::t_float: {
                auto cfl = (ClassFile::ConstantFloat*)c;
                float result;
                auto input = cfl->value_.get();
                memcpy(&result, &input, sizeof(float));
                push_operand(&result);
                break;
            }

            default:
                puts("unhandled ldc...");
                while (true) ;
                break;
            }
            pc += 2;
            break;
        }

        case Bytecode::new_inst:
            push_operand(make_instance(clz, ((network_u16*)&bytecode[pc + 1])->get()));
            // printf("new %p\n", load_operand(0));
            pc += 3;
            break;

        case Bytecode::areturn:
            // NOTE: we implement return values on the stack. nothing to do here.
            return;

        case Bytecode::bipush:
            push_operand((void*)(intptr_t)(int)bytecode[pc + 1]);
            pc += 2;
            break;

        case Bytecode::dup:
            // printf("dup %p\n", load_operand(0));
            push_operand(load_operand(0));
            ++pc;
            break;

        case Bytecode::iconst_0:
            push_operand((void*)(int)0);
            ++pc;
            break;

        case Bytecode::iconst_1:
            push_operand((void*)(int)1);
            ++pc;
            break;

        case Bytecode::iconst_2:
            push_operand((void*)(int)2);
            ++pc;
            break;

        case Bytecode::iconst_3:
            push_operand((void*)(int)3);
            ++pc;
            break;

        case Bytecode::iconst_4:
            push_operand((void*)(int)4);
            ++pc;
            break;

        case Bytecode::iconst_5:
            push_operand((void*)(int)5);
            ++pc;
            break;

        case Bytecode::getfield: {
            auto arg = (Object*)load_operand(0);
            pop_operand();
            push_operand(arg->get_field(((network_u16*)&bytecode[pc + 1])->get()));
            // printf("%d\n", (int)(intptr_t)load_operand(0));
            pc += 3;
            break;
        }

        case Bytecode::putfield:
            ((Object*)load_operand(1))->put_field(((network_u16*)&bytecode[pc + 1])->get(),
                                                  load_operand(0));
            pop_operand();
            pop_operand();
            pc += 3;
            // while (true) ;
            break;

        case Bytecode::isub: {
            const int result = load_operand_i(0) - load_operand_i(1);
            pop_operand();
            pop_operand();
            push_operand((void*)(intptr_t)result);
            pc += 1;
            break;
        }

        case Bytecode::iadd: {
            const int result = load_operand_i(0) + load_operand_i(1);
            pop_operand();
            pop_operand();
            push_operand((void*)(intptr_t)result);
            pc += 1;
            break;
        }

        case Bytecode::idiv: {
            const int result = load_operand_i(0) / load_operand_i(1);
            pop_operand();
            pop_operand();
            push_operand((void*)(intptr_t)result);
            pc += 1;
            break;
        }

        case Bytecode::i2s: {
            short val = load_operand_i(0);
            pop_operand();
            push_operand((void*)(intptr_t)val);
            ++pc;
            break;
        }

        case Bytecode::if_acmpeq:
            if (load_operand(0) == load_operand(1)) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            pop_operand();
            break;

        case Bytecode::if_acmpne:
            if (load_operand(0) not_eq load_operand(1)) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            pop_operand();
            break;

        case Bytecode::if_icmpeq:
            if (load_operand_i(0) == load_operand_i(1)) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            pop_operand();
            break;

        case Bytecode::if_icmpne:
            if (load_operand_i(0) not_eq load_operand_i(1)) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            pop_operand();
            break;

        case Bytecode::if_icmplt:
            if (load_operand_i(0) < load_operand_i(1)) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            pop_operand();
            break;

        case Bytecode::if_icmpge:
            if (load_operand_i(0) > load_operand_i(1)) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            pop_operand();
            break;

        case Bytecode::if_icmple:
            if (load_operand_i(0) <= load_operand_i(1)) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            pop_operand();
            break;

        case Bytecode::if_eq:
            if (load_operand_i(0) == 0) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            break;

        case Bytecode::if_ne:
            if (load_operand_i(0) not_eq 0) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            break;

        case Bytecode::if_lt:
            if (load_operand_i(0) < 0) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            break;

        case Bytecode::if_ge:
            if (load_operand_i(0) >= 0) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            break;

        case Bytecode::if_gt:
            if (load_operand_i(0) > 0) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            break;

        case Bytecode::if_le:
            if (load_operand_i(0) < 0) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            break;

        case Bytecode::if_nonnull:
            if (load_operand(0) not_eq nullptr) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            break;

        case Bytecode::if_null:
            if (load_operand(0) == nullptr) {
                pc += ((network_s16*)(bytecode + pc + 1))->get();
            } else {
                pc += 3;
            }
            pop_operand();
            break;

        case Bytecode::fconst_0: {
            static_assert(sizeof(float) == sizeof(int) and
                          sizeof(void*) >= sizeof(float),
                          "undefined behavior");
            auto f = 0.f;
            push_operand(&f);
            ++pc;
            break;
        }

        case Bytecode::fconst_1: {
            static_assert(sizeof(float) == sizeof(int) and
                          sizeof(void*) >= sizeof(float),
                          "undefined behavior");
            auto f = 1.f;
            push_operand(&f);
            ++pc;
            break;
        }

        case Bytecode::fconst_2: {
            static_assert(sizeof(float) == sizeof(int) and
                          sizeof(void*) >= sizeof(float),
                          "undefined behavior");
            auto f = 2.f;
            push_operand(&f);
            ++pc;
            break;
        }

        case Bytecode::fadd: {
            float lhs = load_operand_f(0);
            float rhs = load_operand_f(1);
            pop_operand();
            pop_operand();
            auto result = lhs + rhs;
            push_operand(&result);
            ++pc;
            break;
        }

        case Bytecode::fdiv: {
            float lhs = load_operand_f(0);
            float rhs = load_operand_f(1);
            pop_operand();
            pop_operand();
            auto result = lhs / rhs;
            push_operand(&result);
            ++pc;
            break;
        }

        case Bytecode::fmul: {
            float lhs = load_operand_f(0);
            float rhs = load_operand_f(1);
            pop_operand();
            pop_operand();
            auto result = lhs * rhs;
            push_operand(&result);
            ++pc;
            break;
        }

        case Bytecode::astore:
        case Bytecode::istore:
            store_local(bytecode[pc + 1], load_operand(0));
            pop_operand();
            pc += 2;
            break;

        case Bytecode::astore_0:
        case Bytecode::istore_0:
            store_local(0, load_operand(0));
            pop_operand();
            ++pc;
            break;

        case Bytecode::astore_1:
        case Bytecode::istore_1:
            store_local(1, load_operand(0));
            // printf("store local 1 %p\n", load_operand(0));
            pop_operand();
            ++pc;
            break;

        case Bytecode::astore_2:
        case Bytecode::istore_2:
            store_local(2, load_operand(0));
            pop_operand();
            ++pc;
            break;

        case Bytecode::astore_3:
        case Bytecode::istore_3:
            store_local(3, load_operand(0));
            pop_operand();
            ++pc;
            break;

        case Bytecode::aload:
        case Bytecode::iload:
            push_operand(load_local(bytecode[pc + 1]));
            pc += 2;
            break;

        case Bytecode::aload_0:
        case Bytecode::iload_0:
            push_operand(load_local(0));
            // printf("load0 %p\n", load_local(0));
            ++pc;
            break;

        case Bytecode::aload_1:
        case Bytecode::iload_1:
            push_operand(load_local(1));
            // printf("load1 %p\n", load_local(1));
            ++pc;
            break;

        case Bytecode::aload_2:
        case Bytecode::iload_2:
            push_operand(load_local(2));
            ++pc;
            break;

        case Bytecode::aload_3:
        case Bytecode::iload_3:
            push_operand(load_local(3));
            ++pc;
            break;

        case Bytecode::iinc:
            store_local(bytecode[pc + 1],
                        (void*)(intptr_t)
                        ((int)(intptr_t)(load_local(bytecode[pc + 1])) +
                         bytecode[pc + 2]));
            // printf("%d\n", (int)(intptr_t)load_local(1));
            pc += 3;
            break;

        case Bytecode::__goto:
            pc += ((network_s16*)(bytecode + pc + 1))->get();
            break;

        case Bytecode::__goto_w:
            pc += ((network_s32*)(bytecode + pc + 1))->get();
            break;

        case Bytecode::vreturn:
            return;

        case Bytecode::invokestatic:
            ++pc;
            dispatch_method(clz,
                            nullptr,
                            ((network_u16*)(bytecode + pc))->get());
            pc += 2;
            break;

        case Bytecode::invokevirtual: {
            ++pc;
            auto obj = (Object*)load_operand(0);
            pop_operand();
            dispatch_method(clz,
                            obj,
                            ((network_u16*)(bytecode + pc))->get());
            pc += 2;
            break;
        }

        case Bytecode::invokespecial:
            ++pc;
            invoke_special(clz, ((network_u16*)(bytecode + pc))->get());
            pc += 2;
            break;

        default:
            printf("unrecognized bytecode instruction %#02x\n", bytecode[pc]);
            for (int i = 0; i < 10; ++i) {
                std::cout << (int)bytecode[i] << std::endl;
            }
            while (true) ;
        }
    }
}



void bootstrap()
{
    // NOTE: I manually edited the bytecode in the Object classfile, which is
    // why I do not provide the source code. It's hand-rolled java bytecode.
    if (parse_classfile(Slice::from_c_str("java/lang/Object"),
                        "Object.class")) {
        puts("successfully loaded Object root!");
    }
}



}



}





////////////////////////////////////////////////////////////////////////////////



int main()
{
    java::jvm::bootstrap();

    if (auto clz = java::parse_classfile(java::Slice::from_c_str("HelloWorldApp"),
                                         "HelloWorldApp.class")) {
        puts("parsed classfile header correctly");

        if (auto entry = clz->load_method("main")) {
            java::jvm::invoke_method(clz, nullptr, entry);
        }

    } else {
        puts("failed to parse class file");
    }
}
