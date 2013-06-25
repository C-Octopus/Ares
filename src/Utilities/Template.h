#ifndef ARES_TEMPLATE_H
#define ARES_TEMPLATE_H

#include <stdexcept>
#include <cstring>

#include <MouseClass.h>
#include <TechnoClass.h>
#include <Helpers/Type.h>
#include "INIParser.h"
#include "Enums.h"

/**
 * More fancy templates!
 * This one is just a nicer-looking INI Parser... the fun starts with the next one
 */

template<typename T>
class Valueable {
protected:
	T    Value;
public:
	typedef T MyType;
	typedef typename CompoundT<T>::BaseT MyBase;
	Valueable(T Default = T()) : Value(Default) {};

	operator T () const {
		return this->Get();
	}

	// only allow this when explict works, otherwise
	// the always-non-null pointer will be used in conditionals.
	//explicit operator T* () {
	//	return this->GetEx();
	//}

	T* operator & () {
		return this->GetEx();
	}

	bool operator ! () const {
		return this->Get() == 0;
	};

	virtual T Get() const {
		return this->Value;
	}

	virtual T * GetEx() {
		return &this->Value;
	}

	virtual void Set(T val) {
		this->Value = val;
	}

	virtual void SetEx(T* val) {
		this->Value = *val;
	}

	void Read(INI_EX *parser, const char* pSection, const char* pKey) {
		ImplementThisFunction();
	};

	void Parse(INI_EX *parser, const char* pSection, const char* pKey, bool Allocate = 0) {
		if(parser->ReadString(pSection, pKey)) {
			const char * val = parser->value();
			if(auto parsed = (Allocate ? MyBase::FindOrAllocate : MyBase::Find)(val)) {
				this->Set(parsed);
			} else {
				Debug::INIParseFailed(pSection, pKey, val);
			}
		}
	}
};

// more fun
template<typename T, typename Lookuper>
class ValueableIdx : public Valueable<T> {
public:
	ValueableIdx(T Default) : Valueable<T>(Default) {};

	void Read(INI_EX *parser, const char* pSection, const char* pKey) {
		if(parser->ReadString(pSection, pKey)) {
			const char * val = parser->value();
			int idx = Lookuper::FindIndex(val);
			if(idx != -1 || INIClass::IsBlank(val)) {
				this->Set(idx);
			} else {
				Debug::INIParseFailed(pSection, pKey, val);
			}
		}
	}
};

template<typename T>
class Nullable : public Valueable<T> {
protected:
	bool HasValue;
public:
	Nullable(): Valueable<T>(T()), HasValue(false) {};
	Nullable(T Val): Valueable<T>(Val), HasValue(true) {};

	bool isset() const {
		return this->HasValue;
	}

	using Valueable<T>::Get;

	T Get(T defVal) const {
		return this->isset() ? Valueable<T>::Get() : defVal;
	}

	using Valueable<T>::GetEx;

	T* GetEx(T* defVal) const {
		return this->isset() ? Valueable<T>::GetEx() : defVal;
	}

	virtual void Set(T val) {
		Valueable<T>::Set(val);
		this->HasValue = true;
	}

	virtual void SetEx(T* val) {
		Valueable<T>::SetEx(val);
		this->HasValue = true;
	}

	void Reset() {
		Valueable<T>::Set(T());
		this->HasValue = false;
	}
};

template<typename T, typename Lookuper>
class NullableIdx : public ValueableIdx<T, Lookuper> {
protected:
	bool HasValue;
public:
	NullableIdx() : ValueableIdx<T, Lookuper>(T()), HasValue(false) {};
	NullableIdx(T Val) : ValueableIdx<T, Lookuper>(Val), HasValue(true) {};

	bool isset() const {
		return this->HasValue;
	}

	using Valueable<T>::Get;

	T Get(T defVal) const {
		return this->isset() ? Valueable<T>::Get() : defVal;
	}

	using Valueable<T>::GetEx;

	T* GetEx(T* defVal) const {
		return this->isset() ? Valueable<T>::GetEx() : defVal;
	}

	virtual void Set(T val) {
		ValueableIdx<T, Lookuper>::Set(val);
		this->HasValue = true;
	}

	virtual void SetEx(T* val) {
		ValueableIdx<T, Lookuper>::SetEx(val);
		this->HasValue = true;
	}

	void Reset() {
		Valueable<T>::Set(T());
		this->HasValue = false;
	}
};

/*
 * This one is for data that defaults to some original flag value but can be overwritten with custom values
 * Bind() it to a data address from where to take the value
 * (e.g. &RulesClass::Global()->RadBeamColor for custom-colorizable rad waves)
 * and Set() it to a fixed value
 */

template<typename T>
class Customizable : public Valueable<T> {
	bool Customized;
	T*   Default;
public:
	Customizable(T* alias = NULL) : Valueable<T>(T()), Customized(false), Default(alias) {};

	void Bind(T* to) {
		if(!this->Customized) {
			this->Default = to;
		}
	}

	void BindEx(T to) {
		if(!this->Customized) {
			this->Value = to;
			this->Default = &this->Value;
		}
	}

	virtual T Get() const {
		return this->Customized
		 ? this->Value
		 : this->Default ? *this->Default : T()
		;
	}

	virtual void Set(T val) {
		this->Customized = true;
		this->Value = val;
	}

	virtual T* GetEx() {
		return this->Customized
		 ? &this->Value
		 : this->Default
		;
	}

	virtual void SetEx(T* val) {
		this->Customized = true;
		this->Value = *val;
	}

	void Lock() {
		if(!this->Customized) {
			if(this->Default) {
				this->Value = *this->Default;
			}
			this->Customized = true;
		}
	}
};

template<typename T, typename Lookuper>
class CustomizableIdx : public ValueableIdx<T, Lookuper> {
	bool Customized;
	T*   Default;
public:
	CustomizableIdx(T* alias = NULL) : ValueableIdx<T, Lookuper>(T()), Customized(false), Default(alias) {};

	void Bind(T* to) {
		if(!this->Customized) {
			this->Default = to;
		}
	}

	void BindEx(T to) {
		if(!this->Customized) {
			this->Value = to;
			this->Default = &this->Value;
		}
	}

	virtual T Get() const {
		return this->Customized
		 ? this->Value
		 : this->Default ? *this->Default : T()
		;
	}

	virtual void Set(T val) {
		this->Customized = true;
		this->Value = val;
	}

	virtual T* GetEx() {
		return this->Customized
		 ? &this->Value
		 : this->Default
		;
	}

	virtual void SetEx(T* val) {
		this->Customized = true;
		this->Value = *val;
	}

	void Lock() {
		if(!this->Customized) {
			if(this->Default) {
				this->Value = *this->Default;
			}
			this->Customized = true;
		}
	}
};
/*
 * This template is for something that varies depending on a unit's Veterancy Level
 * Promotable<int> PilotChance; // class def
 * PilotChance(NULL); // ctor init-list
 * PilotChance->BindTo(Unit); // instantiation
 * PilotChance->Get(); // usage
 */
template<typename T>
class Promotable {
	TechnoClass * _BindTo;
public:
	T Rookie;
	T Veteran;
	T Elite;

	Promotable(TechnoClass * Object = NULL) : _BindTo(Object) {};
	Promotable<T>* BindTo(TechnoClass * Object) {
		this->_BindTo = Object;
		return this;
	}

	void SetAll(T val) {
		this->Elite = this->Veteran = this->Rookie = val;
	}

	void LoadFromINI(CCINIClass *pINI, const char *Section, const char *BaseFlag) {
		unsigned int buflen = strlen(BaseFlag) + 8;
		char *FlagName = new char[buflen];

		Customizable<T> Placeholder;
		INI_EX exINI(pINI);
		Placeholder.Set(this->Rookie);

		_snprintf(FlagName, buflen, BaseFlag, "Rookie");
		Placeholder.Read(&exINI, Section, FlagName);
		this->Rookie = Placeholder.Get();

		Placeholder.Set(this->Veteran);
		_snprintf(FlagName, buflen, BaseFlag, "Veteran");
		Placeholder.Read(&exINI, Section, FlagName);
		this->Veteran = Placeholder.Get();

		Placeholder.Set(this->Elite);
		_snprintf(FlagName, buflen, BaseFlag, "Elite");
		Placeholder.Read(&exINI, Section, FlagName);
		this->Elite = Placeholder.Get();

		delete[] FlagName;
	}

	T* GetEx() {
		if(!this->_BindTo) {
			Debug::Log("Promotable<T> invoked without an owner!\n");
			throw std::logic_error("Promotable<T> invoked without an owner!\n");
		}
		VeterancyStruct *XP = &this->_BindTo->Veterancy;
		if(XP->IsElite()) {
			return &this->Elite;
		}
		if(XP->IsVeteran()) {
			return &this->Veteran;
		}
		return &this->Rookie;
	}

	T Get() {
		return *this->GetEx();
	}
};


// specializations

template<>
void Valueable<bool>::Read(INI_EX *parser, const char* pSection, const char* pKey) {
	bool buffer = this->Get();
	if(parser->ReadBool(pSection, pKey, &buffer)) {
		this->Set(buffer);
	} else if(parser->declared()) {
		Debug::INIParseFailed(pSection, pKey, parser->value(), "Expected a valid boolean value [1, true, yes, 0, false, no]");
	}
};

template<>
void Valueable<int>::Read(INI_EX *parser, const char* pSection, const char* pKey) {
	int buffer = this->Get();
	if(parser->ReadInteger(pSection, pKey, &buffer)) {
		this->Set(buffer);
	} else if(parser->declared()) {
		Debug::INIParseFailed(pSection, pKey, parser->value(), "Expected a valid number");
	}
};

template<>
void Valueable<BYTE>::Read(INI_EX *parser, const char* pSection, const char* pKey) {
	int buffer = this->Get();
	if(parser->ReadInteger(pSection, pKey, &buffer)) {
		if(buffer <= 255 && buffer >= 0) {
			const BYTE result((BYTE)buffer); // shut up shut up shut up C4244
			this->Set(result);
		} else {
			Debug::INIParseFailed(pSection, pKey, parser->value(), "Expected a valid number between 0 and 255 inclusive.");
		}
	} else if(parser->declared()) {
		Debug::INIParseFailed(pSection, pKey, parser->value(), "Expected a valid number");
	}
};

template<>
void Valueable<float>::Read(INI_EX *parser, const char* pSection, const char* pKey) {
	double buffer = this->Get();
	if(parser->ReadDouble(pSection, pKey, &buffer)) {
		this->Set(static_cast<float>(buffer));
	} else if(parser->declared()) {
		Debug::INIParseFailed(pSection, pKey, parser->value(), "Expected a valid floating point number");
	}
};

template<>
void Valueable<double>::Read(INI_EX *parser, const char* pSection, const char* pKey) {
	double buffer = this->Get();
	if(parser->ReadDouble(pSection, pKey, &buffer)) {
		this->Set(buffer);
	} else if(parser->declared()) {
		Debug::INIParseFailed(pSection, pKey, parser->value(), "Expected a valid floating point number");
	}
};

template<>
void Valueable<ColorStruct>::Read(INI_EX *parser, const char* pSection, const char* pKey) {
	ColorStruct buffer = this->Get();
	if(parser->Read3Bytes(pSection, pKey, (byte*)&buffer)) {
		this->Set(buffer);
	} else if(parser->declared()) {
		Debug::INIParseFailed(pSection, pKey, parser->value(), "Expected a valid R,G,B color");
	}
};

template<>
void Valueable<SHPStruct *>::Read(INI_EX *parser, const char* pSection, const char* pKey) {
	if(parser->ReadString(pSection, pKey)) {
		char flag[256];
		const char * val = parser->value();
		_snprintf(flag, 256, "%s.shp", val);
		if(SHPStruct *image = FileSystem::LoadSHPFile(flag)) {
			this->Set(image);
		} else {
			Debug::DevLog(Debug::Warning, "Failed to find file %s referenced by [%s]%s=%s", flag, pSection, pKey, val);
		}
	}
};

template<>
void Valueable<MouseCursor>::Read(INI_EX *parser, const char* pSection, const char* pKey) {
	Customizable<int> Placeholder;

	MouseCursor *Cursor = this->GetEx();

	char pFlagName [32];
	_snprintf(pFlagName, 32, "%s.Frame", pKey);
	Placeholder.Set(Cursor->Frame);
	Placeholder.Read(parser, pSection, pFlagName);
	Cursor->Frame = Placeholder.Get();

	_snprintf(pFlagName, 32, "%s.Count", pKey);
	Placeholder.Set(Cursor->Count);
	Placeholder.Read(parser, pSection, pFlagName);
	Cursor->Count = Placeholder.Get();

	_snprintf(pFlagName, 32, "%s.Interval", pKey);
	Placeholder.Set(Cursor->Interval);
	Placeholder.Read(parser, pSection, pFlagName);
	Cursor->Interval = Placeholder.Get();

	_snprintf(pFlagName, 32, "%s.MiniFrame", pKey);
	Placeholder.Set(Cursor->MiniFrame);
	Placeholder.Read(parser, pSection, pFlagName);
	Cursor->MiniFrame = Placeholder.Get();

	_snprintf(pFlagName, 32, "%s.MiniCount", pKey);
	Placeholder.Set(Cursor->MiniCount);
	Placeholder.Read(parser, pSection, pFlagName);
	Cursor->MiniCount = Placeholder.Get();

	_snprintf(pFlagName, 32, "%s.HotSpot", pKey);
	if(parser->ReadString(pSection, pFlagName)) {
		char *buffer = const_cast<char *>(parser->value());
		char *hotx = strtok(buffer, ",");
		if(!strcmp(hotx, "Left")) this->Value.HotX = hotspx_left;
		else if(!strcmp(hotx, "Center")) this->Value.HotX = hotspx_center;
		else if(!strcmp(hotx, "Right")) this->Value.HotX = hotspx_right;

		if(char *hoty = strtok(NULL, ",")) {
			if(!strcmp(hoty, "Top")) this->Value.HotY = hotspy_top;
			else if(!strcmp(hoty, "Middle")) this->Value.HotY = hotspy_middle;
			else if(!strcmp(hoty, "Bottom")) this->Value.HotY = hotspy_bottom;
		}
	}
};

template<>
void Valueable<RocketStruct>::Read(INI_EX *parser, const char* pSection, const char* pKey) {
	Customizable<bool> BoolPlaceholder;
	Customizable<int> IntPlaceholder;
	Customizable<float> FloatPlaceholder;
	Customizable<AircraftTypeClass*> TypePlaceholder;

	RocketStruct* rocket = this->GetEx();

	char pFlagName[0x40];
	_snprintf(pFlagName, 0x40, "%s.PauseFrames", pKey);
	IntPlaceholder.Set(rocket->PauseFrames);
	IntPlaceholder.Read(parser, pSection, pFlagName);
	rocket->PauseFrames = IntPlaceholder.Get();

	_snprintf(pFlagName, 0x40, "%s.TiltFrames", pKey);
	IntPlaceholder.Set(rocket->TiltFrames);
	IntPlaceholder.Read(parser, pSection, pFlagName);
	rocket->TiltFrames = IntPlaceholder.Get();

	_snprintf(pFlagName, 0x40, "%s.PitchInitial", pKey);
	FloatPlaceholder.Set(rocket->PitchInitial);
	FloatPlaceholder.Read(parser, pSection, pFlagName);
	rocket->PitchInitial = FloatPlaceholder.Get();

	_snprintf(pFlagName, 0x40, "%s.PitchFinal", pKey);
	FloatPlaceholder.Set(rocket->PitchFinal);
	FloatPlaceholder.Read(parser, pSection, pFlagName);
	rocket->PitchFinal = FloatPlaceholder.Get();

	_snprintf(pFlagName, 0x40, "%s.TurnRate", pKey);
	FloatPlaceholder.Set(rocket->TurnRate);
	FloatPlaceholder.Read(parser, pSection, pFlagName);
	rocket->TurnRate = FloatPlaceholder.Get();

	// sic! integer read like a float.
	_snprintf(pFlagName, 0x40, "%s.RaiseRate", pKey);
	FloatPlaceholder.Set(static_cast<float>(rocket->RaiseRate));
	FloatPlaceholder.Read(parser, pSection, pFlagName);
	rocket->RaiseRate = static_cast<int>(Game::F2I(FloatPlaceholder.Get()));

	_snprintf(pFlagName, 0x40, "%s.Acceleration", pKey);
	FloatPlaceholder.Set(rocket->Acceleration);
	FloatPlaceholder.Read(parser, pSection, pFlagName);
	rocket->Acceleration = FloatPlaceholder.Get();

	_snprintf(pFlagName, 0x40, "%s.Altitude", pKey);
	IntPlaceholder.Set(rocket->Altitude);
	IntPlaceholder.Read(parser, pSection, pFlagName);
	rocket->Altitude = IntPlaceholder.Get();

	_snprintf(pFlagName, 0x40, "%s.Damage", pKey);
	IntPlaceholder.Set(rocket->Damage);
	IntPlaceholder.Read(parser, pSection, pFlagName);
	rocket->Damage = IntPlaceholder.Get();
	
	_snprintf(pFlagName, 0x40, "%s.EliteDamage", pKey);
	IntPlaceholder.Set(rocket->EliteDamage);
	IntPlaceholder.Read(parser, pSection, pFlagName);
	rocket->EliteDamage = IntPlaceholder.Get();
	
	_snprintf(pFlagName, 0x40, "%s.BodyLength", pKey);
	IntPlaceholder.Set(rocket->BodyLength);
	IntPlaceholder.Read(parser, pSection, pFlagName);
	rocket->BodyLength = IntPlaceholder.Get();

	_snprintf(pFlagName, 0x40, "%s.LazyCurve", pKey);
	BoolPlaceholder.Set(rocket->LazyCurve);
	BoolPlaceholder.Read(parser, pSection, pFlagName);
	rocket->LazyCurve = BoolPlaceholder.Get();

	_snprintf(pFlagName, 0x40, "%s.Type", pKey);
	TypePlaceholder.Set(rocket->Type);
	TypePlaceholder.Parse(parser, pSection, pFlagName);
	rocket->Type = TypePlaceholder.Get();
};

template<typename T>
class Iterator {
private:
	const T* items;
	size_t count;
public:
	Iterator() : items(nullptr), count(0) {}
	Iterator(const T* first, const size_t count) : items(first), count(count) {}
	Iterator(const std::vector<T> &vec) : items(vec.data()), count(vec.size()) {}
	Iterator(const VectorClass<T> &vec) : items(vec.Items), count(vec.Capacity) {}
	Iterator(const DynamicVectorClass<T> &vec) : items(vec.Items), count(vec.Count) {}

	T at(size_t index) const {
		return this->items[index];
	}

	size_t size() const {
		return this->count;
	}

	const T* begin() const {
		return this->items;
	}

	const T* end() const {
		if(!this->valid()) {
			return nullptr;
		}

		return &this->items[count];
	}

	bool valid() const {
		return items != nullptr;
	}

	bool empty() const {
		return !this->valid() || !this->count;
	}

	bool contains(T other) const {
		return std::find(this->begin(), this->end(), other) != this->end();
	}

	operator bool () const {
		return !this->empty();
	}

	bool operator !() const {
		return this->empty();
	}

	const T& operator [](size_t index) const {
		return this->items[index];
	}
};

template<class T>
class ValueableVector : public std::vector<T> {
protected:
	bool _Defined;
public:
	typedef T MyType;
	typedef typename CompoundT<T>::BaseT MyBase;

	ValueableVector() : std::vector<T>(), _Defined(false) {};

	virtual void Read(INI_EX *parser, const char* pSection, const char* pKey) {
		if(parser->ReadString(pSection, pKey)) {
			this->clear();
			this->_Defined = true;
			this->Split(parser, pSection, pKey, Ares::readBuffer);
		}
	}

	bool Contains(const T &other) const {
		return std::find(this->begin(), this->end(), other) != this->end();
	}

	int IndexOf(const T &other) const {
		auto it = std::find(this->begin(), this->end(), other);
		if(it != this->end()) {
			return it - this->begin();
		}
		return -1;
	}

	bool Defined() const {
		return this->_Defined;
	}

protected:
	virtual void Split(INI_EX *parser, const char* pSection, const char* pKey, char* pValue) {
		// if we were able to get the flag in question, take it apart and check the tokens...
		for(char *cur = strtok(pValue, Ares::readDelims); cur; cur = strtok(nullptr, Ares::readDelims)) {
			Parse(parser, pSection, pKey, cur);
		}
	}

	void Parse(INI_EX *parser, const char* pSection, const char* pKey, char* pValue) {
		T buffer = T();
		if(Parser<T>::Parse(pValue, &buffer)) {
			this->push_back(buffer);
		} else if(!INIClass::IsBlank(pValue)) {
			Debug::INIParseFailed(pSection, pKey, pValue);
		}
	}
};

template<>
void ValueableVector<TechnoTypeClass *>::Parse(INI_EX *parser, const char* pSection, const char* pKey, char* pValue) {
	// ...against the various object types; if we find one, place it in the value list
	if(auto pType = AircraftTypeClass::Find(pValue)) {
		this->push_back(pType);
	} else if(auto pType = BuildingTypeClass::Find(pValue)) {
		this->push_back(pType);
	} else if(auto pType = InfantryTypeClass::Find(pValue)) {
		this->push_back(pType);
	} else if(auto pType = UnitTypeClass::Find(pValue)) {
		this->push_back(pType);
	} else if(!INIClass::IsBlank(pValue)) {
		Debug::INIParseFailed(pSection, pKey, pValue);
	}
}

template<class T>
class NullableVector : public ValueableVector<T> {
protected:
	bool _HasValue;
public:
	NullableVector() : ValueableVector<T>(), _HasValue(false) {};

	virtual void Read(INI_EX *parser, const char* pSection, const char* pKey) {
		if(parser->ReadString(pSection, pKey)) {
			this->clear();
			this->_Defined = true;

			// provide a way to reset to default
			if(!_strcmpi(Ares::readBuffer, "<default>")) {
				this->_HasValue = false;
			} else {
				this->_HasValue = true;
				this->Split(parser, pSection, pKey, Ares::readBuffer);
			}
		}
	}

	bool HasValue() const {
		return this->_HasValue;
	}
};

template<typename T>
class ValueableEnum : public Valueable<typename T::Value> {
public:
	typedef typename T::Value ValueType;

	ValueableEnum(ValueType Default = ValueType()) : Valueable<ValueType>(Default) {};

	void Read(INI_EX *parser, const char* pSection, const char* pKey) {
		if(parser->ReadString(pSection, pKey)) {
			ValueType buffer = this->Get();
			if(T::Parse(Ares::readBuffer, &buffer)) {
				this->Set(buffer);
			} else if(!INIClass::IsBlank(Ares::readBuffer)) {
				Debug::INIParseFailed(pSection, pKey, Ares::readBuffer);
			}
		}
	};
};


//template class Valueable<bool>;
//template class Valueable<int>;
//template class Valueable<double>;
//template class Valueable<ColorStruct>;
//template class Valueable<SHPStruct *>;
//template class Valueable<MouseCursor>;

#endif
