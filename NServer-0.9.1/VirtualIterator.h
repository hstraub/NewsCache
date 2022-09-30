#ifndef _ITER_H_
#define _ITER_H_

/**
 * \class _Iter
 * \author Thomas Gschwind
 * \bug Documentation is missing.
 */
template < class T > class _Iter {
  protected:
	int _type;

  public:
	virtual ~ _Iter() {
	};
	virtual _Iter *clone() = 0;
	virtual T *get() = 0;
	virtual void next() = 0;
	virtual bool equals(_Iter * i2) = 0;
};

/**
 * \class Iter
 * \author Thomas Gschwind
 * \bug Documentation is missing.
 */
template < class T > class Iter {
  private:
	_Iter < T > *iter;

      Iter():iter(NULL) {
	}
      public:
      Iter(_Iter < T > *iiter):iter(iiter) {
	}
	~Iter() {
		delete iter;
	}

	Iter(const Iter & i2) {
		iter = i2.iter->clone();
	}

	Iter & operator=(const Iter & i2) {
		delete iter;
		iter = i2.iter->clone();
	}

	T operator*() {
		return *iter->get();
	}

	T *operator->() {
		return iter->get();
	}

	Iter & operator++() {	//prefix
		iter->next();
		return *this;
	}

	Iter operator++(int) {	//postfix
		Iter curr(*this);
		iter->next();
		return curr;
	}

	bool operator==(const Iter & i2) {
		return iter->equals(i2.iter);
	}

	bool operator!=(const Iter & i2) {
		return !iter->equals(i2.iter);
	}
};

#endif

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
