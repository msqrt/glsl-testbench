#pragma once

#include <Windows.h>

#include <string>

#include <gl/gl.h>
#include "loadgl/glext.h"
#include "loadgl/loadgl46.h"

struct TimeStamp {
	GLuint synchronousObject, available = GL_FALSE;
	GLint64 synchronous, asynchronous;
	TimeStamp() {
		glCreateQueries(GL_TIMESTAMP, 1, &synchronousObject);
		glGetInteger64v(GL_TIMESTAMP, &asynchronous);
		glQueryCounter(synchronousObject, GL_TIMESTAMP);
	}
	inline void check() {
		if (available == GL_TRUE)
			return;
		while (available == GL_FALSE)
			glGetQueryObjectuiv(synchronousObject, GL_QUERY_RESULT_AVAILABLE, &available);
		glGetQueryObjecti64v(synchronousObject, GL_QUERY_RESULT, &synchronous);
	}
	~TimeStamp() {
		glDeleteQueries(1, &synchronousObject);
	}
};

// ms
inline double latency(TimeStamp& stamp) {
	stamp.check();
	return double(stamp.synchronous - stamp.asynchronous)*1.0e-6;
}

// ms
inline double elapsedTime(TimeStamp& begin, TimeStamp& end) {
	end.check();
	if (begin.available != GL_TRUE) {
		begin.available = GL_TRUE;
		glGetQueryObjecti64v(begin.synchronousObject, GL_QUERY_RESULT, &begin.synchronous);
	}
	return double(end.synchronous - begin.synchronous)*1.0e-6;
}