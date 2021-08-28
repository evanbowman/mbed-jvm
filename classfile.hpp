#pragma once

#include "endian.hpp"
#include <stdio.h>
#include "slice.hpp"



namespace java {



struct ClassFile {
    struct HeaderSection1 {
        network_u32 magic_;
        network_u16 minor_version_;
        network_u16 major_version_;
        network_u16 constant_count_;
    };

    struct HeaderSection2 {
        network_u16 access_flags_;
        network_u16 this_class_;
        network_u16 super_class_;
        network_u16 interfaces_count_;
    };

    struct HeaderSection3 {
        network_u16 fields_count_;
    };

    struct HeaderSection4 {
        network_u16 methods_count_;
    };

    struct HeaderSection5 {
        network_u16 attributes_count_;
    };

    struct MethodInfo {
        network_u16 access_flags_;
        network_u16 name_index_;
        network_u16 descriptor_index_;
        network_u16 attributes_count_;
        // AttributeInfo attributes[attributes_count_];
    };

    struct FieldInfo {
        network_u16 access_flags_;
        network_u16 name_index_;
        network_u16 descriptor_index_;
        network_u16 attributes_count_;
        // AttributeInfo attributes[attribute_count_];
    };

    struct AttributeInfo {
        network_u16 attribute_name_index_;
        network_u32 attribute_length_;
        // u8 info[attribute_length_];
    };

    struct AttributeCode {
        AttributeInfo header_;
        network_u16 max_stack_;
        network_u16 max_locals_;
        network_u32 code_length_;
        // u8 bytecode[code_length_];
        // __exception_table
    };

    struct AttributeSourceFile {
        AttributeInfo header_;
        network_u16 sourcefile_index_;
    };


    enum ConstantType : u8 {
        t_class = 7,
        t_field_ref = 9,
        t_method_ref = 10,
        t_interface_method_ref = 11,
        t_string = 8,
        t_integer = 3,
        t_float = 4,
        t_long = 5,
        t_double = 6,
        t_name_and_type = 12,
        t_utf8 = 1,
        t_method_handle = 15,
        t_method_type = 16,
        t_invoke_dynamic = 18,
    };

    // u8 rest_[...];

    struct ConstantHeader {
        ConstantType tag_;
    };

    struct ConstantClass {
        ConstantHeader header_;
        network_u16 name_index_;
    };

    struct ConstantRef {
        // NOTE: used for FieldRef, MethodRef, and InterfaceMethodRef
        ConstantHeader header_;
        network_u16 class_index_;
        network_u16 name_and_type_index_;
    };

    struct ConstantString {
        ConstantHeader header_;
        network_u16 string_index_;
    };

    struct ConstantInteger {
        ConstantHeader header_;
        network_u32 value_;
    };

    struct ConstantFloat {
        ConstantHeader header_;
        network_u32 value_;
    };

    struct ConstantLong {
        ConstantHeader header_;
        network_u32 high_bytes_;
        network_u32 low_bytes_;
    };

    struct ConstantDouble {
        ConstantHeader header_;
        network_u32 high_bytes_;
        network_u32 low_bytes_;
    };

    struct ConstantNameAndType {
        ConstantHeader header_;
        network_u16 name_index_;
        network_u16 descriptor_index_;
    };

    struct ConstantUtf8 {
        ConstantHeader header_;
        network_u16 length_;
        // u8 bytes[length_]
    };

    struct ConstantMethodHandle {
        ConstantHeader header_;
        u8 reference_kind_;
        network_u16 reference_index_;
    };

    struct ConstantMethodType {
        ConstantHeader header_;
        network_u16 descriptor_index_;
    };

    struct ConstantInvokeDynamic {
        ConstantHeader header_;
        network_u16 bootstrap_method_attr_index_;
        network_u16 name_and_type_index_;
    };


    static inline size_t constant_size(const ConstantHeader* hdr)
    {
        switch (hdr->tag_) {
        default:
            printf("error, constant %d\n", hdr->tag_);
            while (true) ;
            break;

        case ClassFile::ConstantType::t_class:
            return sizeof(ClassFile::ConstantClass);

        case ClassFile::ConstantType::t_field_ref:
            return sizeof(ClassFile::ConstantRef);

        case ClassFile::ConstantType::t_method_ref:
            return sizeof(ClassFile::ConstantRef);

        case ClassFile::ConstantType::t_interface_method_ref:
            return sizeof(ClassFile::ConstantRef);

        case ClassFile::ConstantType::t_string:
            return sizeof(ClassFile::ConstantString);

        case ClassFile::ConstantType::t_integer:
            return sizeof(ClassFile::ConstantInteger);

        case ClassFile::ConstantType::t_float:
            return sizeof(ClassFile::ConstantFloat);

        case ClassFile::ConstantType::t_long:
            return sizeof(ClassFile::ConstantLong);

        case ClassFile::ConstantType::t_double:
            return sizeof(ClassFile::ConstantDouble);

        case ClassFile::ConstantType::t_name_and_type:
            return sizeof(ClassFile::ConstantNameAndType);

        case ClassFile::ConstantType::t_utf8:
            return sizeof(ClassFile::ConstantUtf8)
                + ((const ClassFile::ConstantUtf8*)hdr)->length_.get();

        case ClassFile::ConstantType::t_method_handle:
            return sizeof(ClassFile::ConstantMethodHandle);

        case ClassFile::ConstantType::t_method_type:
            return sizeof(ClassFile::ConstantMethodType);

        case ClassFile::ConstantType::t_invoke_dynamic:
            return sizeof(ClassFile::ConstantInvokeDynamic);
        }
    }
};



struct Class;



Class* parse_classfile(Slice classname, const char* name);



}
