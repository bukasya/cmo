#include "stdafx.h"
#include "source.h"


Source::Source(int number)
{
	Source::number = number;
}

Source::~Source()
{
}

Request Source::generate_request()
{
	return Request(get_number()); //create request with number of current source
}

int Source::get_number()
{
	return number;
}
