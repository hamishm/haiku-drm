/*
 * Copyright 2015, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FUTURE_H
#define _FUTURE_H


template<typename Type>
class BPromise {
public:
			BPromise() { }
			~BPromise() { }

private:
	Type	fType;
	std::function 
};


template<typename... Type>
class BFuture {
	BFuture(BPromise& promise) : fPromise(promise) { }
	~BFuture

private:
	BPromise&	fPromise;
};

#endif
