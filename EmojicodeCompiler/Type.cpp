//
//  Type.c
//  Emojicode
//
//  Created by Theo Weidmann on 04.03.15.
//  Copyright (c) 2015 Theo Weidmann. All rights reserved.
//

#include "EmojicodeCompiler.hpp"
#include "utf8.h"
#include "Lexer.hpp"
#include "Class.hpp"
#include "Procedure.hpp"
#include <cstring>

//MARK: Globals
/* Very important one time declarations */

Class *CL_STRING;

Class *CL_LIST;

Class *CL_ERROR;

Class *CL_DATA;

Class *CL_DICTIONARY;

Protocol *PR_ENUMERATEABLE;

Class *CL_ENUMERATOR;

Type Type::typeConstraintForReference(TypeContext ct){
    Type t = *this;
    bool optional = t.optional;
    while (t.type == TT_LOCAL_REFERENCE) {
        t = ct.p->genericArgumentContraints[t.reference];
    }
    while (t.type == TT_REFERENCE) {
        t = ct.normalType.eclass->genericArgumentContraints[t.reference];
    }
    t.optional = optional;
    return t;
}

Type Type::resolveOnSuperArguments(Class *c, bool *resolved){
    Type t = *this;
    while (true) {
        if (t.type != TT_REFERENCE || !c->superclass || t.reference >= c->superclass->genericArgumentCount) {
            return t;
        }
        *resolved = true;
        t = c->superGenericArguments[t.reference];
    }
}

/** Returns the name of a type */

bool Type::compatibleTo(Type to, TypeContext ct){
    //(to.optional || !a.optional): Either `to` accepts optionals, or if `to` does not accept optionals `a` mustn't be one.
    if (to.type == TT_SOMETHING) {
        return true;
    }
    else if (to.type == TT_SOMEOBJECT && (this->type == TT_CLASS || this->type == TT_PROTOCOL || this->type == TT_SOMEOBJECT)){
        return to.optional || !this->optional;
    }
    else if (this->type == TT_CLASS && to.type == TT_CLASS){
        if ((to.optional || !this->optional) && this->eclass->inheritsFrom(to.eclass)) {
            if (to.eclass->ownGenericArgumentCount) {
                for (int l = to.eclass->ownGenericArgumentCount, i = to.eclass->genericArgumentCount - l; i < l; i++) {
                    if (!this->genericArguments[i].compatibleTo(to.genericArguments[i], ct)) {
                        return false;
                    }
                }
            }
            return true;
        }
        return false;
    }
    else if (this->type == TT_PROTOCOL && to.type == TT_PROTOCOL) {
        return (to.optional || !this->optional) && this->protocol == to.protocol;
    }
    else if (this->type == TT_CLASS && to.type == TT_PROTOCOL) {
        return (to.optional || !this->optional) && this->eclass->conformsTo(to.protocol);
    }
    else if (this->type == TT_NOTHINGNESS) {
        return to.optional || to.type == TT_NOTHINGNESS;
    }
    else if (this->type == TT_ENUM && to.type == TT_ENUM) {
        return (to.optional || !this->optional) && this->eenum == to.eenum;
    }
    else if ((this->type == TT_REFERENCE && to.type == TT_REFERENCE) || (this->type == TT_LOCAL_REFERENCE && to.type == TT_LOCAL_REFERENCE)) {
        if((to.optional || !this->optional) && this->reference == to.reference) {
            return true;
        }
        return (to.optional || !this->optional) && this->typeConstraintForReference(ct).compatibleTo(to.typeConstraintForReference(ct), ct);
    }
    else if (this->type == TT_REFERENCE) {
        bool resolved = false;
        Type rt = this->resolveOnSuperArguments(ct.normalType.eclass, &resolved);
        if(resolved && (to.optional || !this->optional) && rt.compatibleTo(to, ct)) {
            return true;
        }
        return (to.optional || !this->optional) && this->typeConstraintForReference(ct).compatibleTo(to, ct);
    }
    else if (to.type == TT_REFERENCE) {
        bool resolved = false;
        Type rt = to.resolveOnSuperArguments(ct.normalType.eclass, &resolved);
        if(resolved && (to.optional || !this->optional) && this->compatibleTo(rt, ct)) {
            return true;
        }
        return (to.optional || !this->optional) && this->compatibleTo(to.typeConstraintForReference(ct), ct);
    }
    else if (this->type == TT_LOCAL_REFERENCE) {
        return (to.optional || !this->optional) && this->typeConstraintForReference(ct).compatibleTo(to, ct);
    }
    else if (to.type == TT_LOCAL_REFERENCE) {
        return (to.optional || !this->optional) && this->compatibleTo(to.typeConstraintForReference(ct), ct);
    }
    else if (this->type == TT_CALLABLE && to.type == TT_CALLABLE) {
        if (this->genericArguments[0].compatibleTo(to.genericArguments[0], ct) && to.arguments == this->arguments) {
            for (int i = 1; i <= to.arguments; i++) {
                if (!to.genericArguments[i].compatibleTo(this->genericArguments[i], ct)) {
                    return false;
                }
            }
            return true;
        }
        return false;
    }
    else {
        return (to.optional || !this->optional) && this->type == to.type;
    }
    return false;
}

Type Type::fetchRawType(EmojicodeChar name, EmojicodeChar enamespace, bool optional, const Token *token, bool *existent){
    *existent = true;
    if(enamespace == globalNamespace){
        switch (name) {
            case E_OK_HAND_SIGN:
                return Type(TT_BOOLEAN, optional);
            case E_INPUT_SYMBOL_FOR_SYMBOLS:
                return Type(TT_SYMBOL, optional);
            case E_STEAM_LOCOMOTIVE:
                return Type(TT_INTEGER, optional);
            case E_ROCKET:
                return Type(TT_DOUBLE, optional);
            case E_MEDIUM_WHITE_CIRCLE:
                if (optional) {
                    compilerWarning(token, "🍬⚪️ is identical to ⚪️. Do not specify 🍬.");
                }
                return Type(TT_SOMETHING, false);
            case E_LARGE_BLUE_CIRCLE:
                return Type(TT_SOMEOBJECT, optional);
            case E_SPARKLES:
                compilerError(token, "The Nothingness type may not be referenced to.");
        }
    }
    
    Class *eclass = getClass(name, enamespace);
    if (eclass) {
        Type t = Type(eclass, optional);
        t.optional = optional;
        return t;
    }
    
    Protocol *protocol = getProtocol(name, enamespace);
    if(protocol){
        return Type(protocol, optional);
    }
    
    Enum *eenum = getEnum(name, enamespace);
    if(eenum){
        return Type(eenum, optional);
    }
    
    *existent = false;
    return typeNothingness;
}

Type Type::resolveOn(TypeContext typeContext){
    Type t = *this;
    bool optional = t.optional;
    while (t.type == TT_LOCAL_REFERENCE) {
        t = (*typeContext.procedureGenericArguments)[t.reference];
    }
    while (t.type == TT_REFERENCE) {
        t = typeContext.normalType.genericArguments[t.reference];
    }
    t.optional = optional;
    if (t.type == TT_CLASS) {
        for (int i = 0; i < t.eclass->genericArgumentCount; i++) {
            t.genericArguments[i] = t.genericArguments[i].resolveOn(typeContext);
        }
    }
    else if (t.type == TT_CALLABLE) {
        for (int i = 0; i < t.arguments + 1; i++) {
            t.genericArguments[i] = t.genericArguments[i].resolveOn(typeContext);
        }
    }
    return t;
}

//MARK: Type Parsing Utility

const Token* Type::parseTypeName(EmojicodeChar *typeName, EmojicodeChar *enamespace, bool *optional, EmojicodeChar currentNamespace){
    if (nextToken()->type == VARIABLE) {
        compilerError(consumeToken(), "Generic variables not allowed here.");
    }
    auto *className = consumeToken(IDENTIFIER);
    
    if(className->value[0] == E_CANDY){
        *optional = true;
        
        className = consumeToken(IDENTIFIER);
    }
    else {
        *optional = false;
    }
    
    if(className->value[0] == E_ORANGE_TRIANGLE){
        const Token *nsToken = consumeToken(IDENTIFIER);
        *enamespace = nsToken->value[0];
        
        className = consumeToken(IDENTIFIER);
    }
    else {
        *enamespace = currentNamespace;
    }
    
    *typeName = className->value[0];
    
    return className;
}

Type Type::parseAndFetchType(TypeContext ct, EmojicodeChar theNamespace, TypeDynamism dynamism, bool *dynamicType){
    if (dynamicType) {
        *dynamicType = false;
    }
    if (dynamism & AllowGenericTypeVariables && (ct.normalType.type == TT_CLASS || ct.p)
        && (nextToken()->type == VARIABLE || (nextToken()->value[0] == E_CANDY && nextToken()->nextToken->type == VARIABLE))) {
        if (dynamicType) {
            *dynamicType = true;
        }
        
        bool optional = false;
        const Token *variableToken = consumeToken();
        if (variableToken->value[0] == E_CANDY) {
            variableToken = consumeToken();
            optional = true;
        }
        
        if (ct.p) {
            auto it = ct.p->genericArgumentVariables.find(variableToken->value);
            if (it != ct.p->genericArgumentVariables.end()){
                Type type = it->second;
                type.optional = optional;
                return type;
            }
        }
        if (ct.normalType.type == TT_CLASS) {
            auto it = ct.normalType.eclass->ownGenericArgumentVariables.find(variableToken->value);
            if (it != ct.normalType.eclass->ownGenericArgumentVariables.end()){
                Type type = it->second;
                type.optional = optional;
                return type;
            }
        }
            
        compilerError(variableToken, "No such generic type variable \"%s\".", variableToken->value.utf8CString());
    }
    else if (nextToken()->value[0] == E_RAT) {
        const Token *token = consumeToken();
        
        if(!(dynamism & AllowDynamicClassType)){
            compilerError(token, "🐀 not allowed here.");
        }
        
        if (dynamicType) {
            *dynamicType = true;
        }
        return ct.normalType;
    }
    else if (nextToken()->value[0] == E_GRAPES || (nextToken()->value[0] == E_CANDY && nextToken()->nextToken->type == VARIABLE)) {
        bool optional = false;
        if (nextToken()->value[0] == E_CANDY) {
            consumeToken();
            optional = true;
        }
        consumeToken();
        
        Type t(TT_CALLABLE, optional);
        t.arguments = 0;
        
        t.genericArguments.push_back(typeNothingness);
        
        while (!(nextToken()->type == IDENTIFIER && (nextToken()->value[0] == E_WATERMELON || nextToken()->value[0] == E_RIGHTWARDS_ARROW))) {
            t.arguments++;
            t.genericArguments.push_back(parseAndFetchType(ct, theNamespace, dynamism, nullptr));
        }
        
        if(nextToken()->type == IDENTIFIER && nextToken()->value[0] == E_RIGHTWARDS_ARROW){
            consumeToken();
            t.genericArguments[0] = parseAndFetchType(ct, theNamespace, dynamism, nullptr);
        }
        
        const Token *token = consumeToken(IDENTIFIER);
        if (token->value[0] != E_WATERMELON) {
            compilerError(token, "Expected 🍉.");
        }
        
        return t;
    }
    else {
        EmojicodeChar typeName, typeNamespace;
        bool optional, existent;
        const Token *token = parseTypeName(&typeName, &typeNamespace, &optional, theNamespace);
        
        Type type = fetchRawType(typeName, typeNamespace, optional, token, &existent);
        
        if (!existent) {
            ecCharToCharStack(typeName, nameString);
            ecCharToCharStack(typeNamespace, namespaceString);
            compilerError(token, "Could not find type %s in enamespace %s.", nameString, namespaceString);
        }
        
        type.parseGenericArguments(ct, theNamespace, dynamism, token);
        
        return type;
    }
}

void Type::validateGenericArgument(Type ta, uint16_t i, TypeContext ct, const Token *token){
    if (this->type != TT_CLASS) {
        compilerError(token, "The compiler encountered an internal inconsistency related to generics.");
    }
    if (this->eclass->superclass) {
        i += this->eclass->superclass->genericArgumentCount;
    }
    if (!ta.compatibleTo(this->eclass->genericArgumentContraints[i], ct)) {
        compilerError(token, "Types not matching.");
    }
}

void Type::parseGenericArguments(TypeContext ct, EmojicodeChar theNamespace, TypeDynamism dynamism, const Token *errorToken) {
    if (this->type == TT_CLASS) {
        this->genericArguments = std::vector<Type>(this->eclass->superGenericArguments);
        if (this->eclass->ownGenericArgumentCount){
            int count = 0;
            while(nextToken()->value[0] == E_SPIRAL_SHELL){
                const Token *token = consumeToken();
                
                Type ta = parseAndFetchType(ct, theNamespace, dynamism, nullptr);
                validateGenericArgument(ta, count, ct, token);
                genericArguments.push_back(ta);
                
                count++;
            }
            
            if(count != this->eclass->ownGenericArgumentCount){
                auto str = this->toString(typeNothingness, false);
                compilerError(errorToken, "Type %s requires %d generic arguments, but %d were given.", str.c_str(), this->eclass->ownGenericArgumentCount, count);
            }
        }
    }
}

//MARK: Type Interferring

void CommonTypeFinder::addType(Type t, TypeContext typeContext){
    if (!firstTypeFound) {
        commonType = t;
        firstTypeFound = true;
    }
    else if (!t.compatibleTo(commonType, typeContext)) {
        if (commonType.compatibleTo(t, typeContext)) {
            commonType = t;
        }
        else if(t.type == TT_CLASS && commonType.type == TT_CLASS) {
            commonType = typeSomeobject;
        }
        else {
            commonType = typeSomething;
        }
    }
}

Type CommonTypeFinder::getCommonType(const Token *warningToken){
    if(!firstTypeFound){
        compilerWarning(warningToken, "Type is ambigious without more context.");
    }
    return commonType;
}

//MARK: Type Visulisation

const char* Type::typePackage(){
    switch (this->type) {
        case TT_CLASS:
            return this->eclass->package->name;
        case TT_PROTOCOL:
            return this->protocol->package->name;
        case TT_ENUM:
            return this->eenum->package.name;
        case TT_INTEGER:
        case TT_NOTHINGNESS:
        case TT_BOOLEAN:
        case TT_SYMBOL:
        case TT_DOUBLE:
        case TT_SOMETHING:
        case TT_SOMEOBJECT:
        case TT_REFERENCE:
        case TT_LOCAL_REFERENCE:
        case TT_CALLABLE:
            return "";
    }
}

void stringAppendEc(EmojicodeChar c, std::string *string){
    ecCharToCharStack(c, sc);
    string->append(sc);
}

Type::Type(Class *c, bool o) : optional(o), type(TT_CLASS), eclass(c) {
    for (int i = 0; i < eclass->genericArgumentCount; i++) {
        genericArguments.push_back(Type(TT_REFERENCE, false, i));
    }
}

void Type::typeName(Type type, TypeContext typeContext, bool includeNsAndOptional, std::string *string) const {
    if (includeNsAndOptional) {
        if(type.optional){
            stringAppendEc(E_CANDY, string);
        }
        
        switch (type.type) {
            case TT_CLASS:
                stringAppendEc(type.eclass->enamespace, string);
                break;
            case TT_PROTOCOL:
                stringAppendEc(type.protocol->enamespace, string);
                break;
            case TT_CALLABLE:
            case TT_LOCAL_REFERENCE:
            case TT_REFERENCE:
                break;
            default:
                stringAppendEc(E_LARGE_RED_CIRCLE, string);
                break;
        }
    }
    
    switch (type.type) {
        case TT_CLASS: {
            stringAppendEc(type.eclass->name, string);
            
            int offset = type.eclass->genericArgumentCount - type.eclass->ownGenericArgumentCount;
            for (int i = 0, l = type.eclass->ownGenericArgumentCount; i < l; i++) {
                stringAppendEc(E_SPIRAL_SHELL, string);
                typeName(type.genericArguments[offset + i], typeContext, includeNsAndOptional, string);
            }
            
            return;
        }
        case TT_PROTOCOL:
            stringAppendEc(type.protocol->name, string);
            return;
        case TT_ENUM:
            stringAppendEc(type.eenum->name, string);
            return;
        case TT_INTEGER:
            stringAppendEc(E_STEAM_LOCOMOTIVE, string);
            return;
        case TT_NOTHINGNESS:
            stringAppendEc(E_SPARKLES, string);
            return;
        case TT_BOOLEAN:
            stringAppendEc(E_OK_HAND_SIGN, string);
            return;
        case TT_SYMBOL:
            stringAppendEc(E_INPUT_SYMBOL_FOR_SYMBOLS, string);
            return;
        case TT_DOUBLE:
            stringAppendEc(E_ROCKET, string);
            return;
        case TT_SOMETHING:
            stringAppendEc(E_MEDIUM_WHITE_CIRCLE, string);
            return;
        case TT_SOMEOBJECT:
            stringAppendEc(E_LARGE_BLUE_CIRCLE, string);
            return;
        case TT_CALLABLE:
            stringAppendEc(E_GRAPES, string);
            
            for (int i = 1; i <= type.arguments; i++) {
                typeName(type.genericArguments[i], typeContext, includeNsAndOptional, string);
            }
            
            stringAppendEc(E_RIGHTWARDS_ARROW, string);
            stringAppendEc(0xFE0F, string);
            typeName(type.genericArguments[0], typeContext, includeNsAndOptional, string);
            stringAppendEc(E_WATERMELON, string);
            return;
        case TT_REFERENCE: {
            if (typeContext.normalType.type == TT_CLASS) {
                Class *eclass = typeContext.normalType.eclass;
                do {
                    for (auto it : eclass->ownGenericArgumentVariables) {
                        if (it.second.reference == type.reference) {
                            string->append(it.first.utf8CString());
                            return;
                        }
                    }
                } while ((eclass = eclass->superclass));
            }
            
            stringAppendEc('T', string);
            stringAppendEc('0' + type.reference, string);
            return;
        }
        case TT_LOCAL_REFERENCE:
            if (typeContext.p) {
                for (auto it : typeContext.p->genericArgumentVariables) {
                    if (it.second.reference == type.reference) {
                        string->append(it.first.utf8CString());
                        return;
                    }
                }
            }
            
            stringAppendEc('L', string);
            stringAppendEc('0' + type.reference, string);
            return;
    }
}

std::string Type::toString(TypeContext typeContext, bool includeNsAndOptional) const {
    std::string string;
    typeName(*this, typeContext, includeNsAndOptional, &string);
    return string;
}
