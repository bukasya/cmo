#include "stdafx.h"
#include "source.h"


Source::Source(int number)
{
	Source::number = number;
	Source::generated_reqs = 0;
	Source::declined_reqs = 0;
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

int Source::get_generated_reqs()
{
	return this->generated_reqs;
}

void Source::increment_generated_reqs()
{
	this->generated_reqs += 1;
}

int Source::get_declined_reqs()
{
	return this->declined_reqs;
}

void Source::increment_declined_reqs()
{
	this->declined_reqs += 1;
}
