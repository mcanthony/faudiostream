#include "xtended.hh"
#include "compatibility.hh"
#include "Text.hh"
#include <math.h>vector<::Type>

#include "floats.hh"
#include "code_container.hh"

class RemainderPrim : public xtended
{

 public:

 	RemainderPrim() : xtended("remainder") {}

	virtual unsigned int arity () { return 2; }

	virtual bool	needCache ()	{ return true; }

	virtual ::Type 	infereSigType (const vector< ::Type>& args)
	{
		assert (args.size() == arity());
		return castInterval(floatCast(args[0]|args[1]), interval());   // temporary rule !!!
	}

	virtual void 	sigVisit (Tree sig, sigvisitor* visitor) {}

	virtual int infereSigOrder (const vector<int>& args) {
		assert (args.size() == arity());
		return max(args[0], args[1]);
	}

	virtual Tree	computeSigOutput (const vector<Tree>& args) {
		num n,m;
		assert (args.size() == arity());
		if (isNum(args[0],n) & isNum(args[1],m)) {
			return tree(remainder(double(n), double(m)));
		} else {
			return tree(symbol(), args[0], args[1]);
		}
	}

    virtual ValueInst* generateCode(CodeContainer* container, const list<ValueInst*>& args, ::Type result, vector< ::Type> const & types)
    {
        assert(args.size() == arity());
		assert(types.size() == arity());
        
        Typed::VarType result_type;
        vector<Typed::VarType> arg_types;
        list<ValueInst*> casted_args;
        prepareTypeArgsResult(result, args, types, result_type, arg_types, casted_args);

        return container->pushFunction(subst("remainder$0", isuffix()), result_type, arg_types, args);
    }

	virtual string 	generateLateq (Lateq* lateq, const vector<string>& args, const vector< ::Type>& types)
	{
		assert (args.size() == arity());
		assert (types.size() == arity());

		return subst("$0\\pmod{$1}", args[0], args[1]); // Same as fmodprim.cpp.
	}

};
