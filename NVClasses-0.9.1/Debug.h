#ifndef _Debug_h_
#define _Debug_h_
#include <fstream>

#include "config.h"
#include "Logger.h"
// Insert the following lines into your main app
//TRACE(ofstream ctrace("trace"));
//DEBUG(ofstream cdbg("debug"));
//int verbose=0
//int verbose_what=0

// Verbosity levels
// See logger.h

#ifdef ENABLE_DEBUG
extern Logger slog;
#define DEBUG(x) x
#define VERB(x) x
#define VERBLV(lv,x) if(lv<verbose) { x; }
#define VERBTY(w,x) if(verbose_what==w) { x; }
#else
#define DEBUG(x)
#define VERB(x)
#define VERBLV(lv,x)
#define VERBTY(w,x)
#endif

#ifdef ENABLE_ASSERTIONS
extern Logger slog;
#define ASSERT(x) x
#define ASSERT2(x)
#else
#define ASSERT(x)
#define ASSERT2(x)
#endif

#define NC_CATCH_ALL(text) \
catch(const UsageError &e) \
{ \
	slog.p(Logger::Error) << text << " caught " << "UsageError "; \
	e.print(); \
	throw; \
} \
 \
catch(const NotAllowedError &e) \
{ \
	slog.p(Logger::Error) << text << " caught " << "NotAllowdError "; \
	e.print(); \
	throw; \
} \
 \
catch(const NoSuchArticleError &e) \
{ \
	slog.p(Logger::Error) << text << " caught " << "NoSuchArticleError "; \
	e.print(); \
	throw; \
} \
 \
catch(const DuplicateArticleError &e) \
{ \
	slog.p(Logger::Error) << text << " caught " \
	    << "DuplicateArticleError "; \
	e.print(); \
	throw; \
} \
 \
catch(const NoSuchGroupError &e) \
{ \
	slog.p(Logger::Error) << text << " caught " << "NoSuchGroupError "; \
	e.print(); \
	throw; \
} \
 \
catch(const NoNewsServerError &e) \
{ \
	slog.p(Logger::Error) << text << " caught " << "NoNewsServerError "; \
	e.print(); \
	throw; \
} \
 \
catch(const NoSuchFieldError &e) \
{ \
	slog.p(Logger::Error) << text << " caught " << "NoSuchFieldError "; \
	e.print(); \
	throw; \
} \
 \
catch(const NSError &e) \
{ \
	slog.p(Logger::Error) << text << " caught " << "NSError "; \
	e.print(); \
	throw; \
} \
 \
catch(const AssertionError &e) \
{ \
	slog.p(Logger::Error) << text << " caught " << "AssertionError "; \
	e.print(); \
	throw; \
} \
 \
catch(const IOError &e) \
{ \
	slog.p(Logger::Error) << text << " caught " << "IOError "; \
	e.print(); \
	throw; \
} \
 \
catch(const SystemError &e) \
{ \
	slog.p(Logger::Error) << text << " caught " << "SystemError "; \
	e.print(); \
	throw; \
} \
 \
catch(const Error &e) \
{ \
	slog.p(Logger::Error) << text << " caught " << "Error "; \
	e.print(); \
	throw; \
} \
 \
catch(...) \
{ \
	slog.p(Logger::Error) << text << " caught " << "unknown!!" << "\n"; \
	throw; \
}

#endif

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
