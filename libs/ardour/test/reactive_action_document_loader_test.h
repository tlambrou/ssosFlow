#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveActionDocumentLoaderTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveActionDocumentLoaderTest);
	CPPUNIT_TEST (preferSessionDocumentOverUserDocument);
	CPPUNIT_TEST (loadUserDocumentWhenSessionDocumentMissing);
	CPPUNIT_TEST (useFallbackSeedWhenNoDocumentFileExists);
	CPPUNIT_TEST (reportConfiguredDocumentParseFailureWithoutFallback);
	CPPUNIT_TEST (describeLoadStatusForConfiguredAndFallbackDocuments);
	CPPUNIT_TEST_SUITE_END ();

public:
	void preferSessionDocumentOverUserDocument ();
	void loadUserDocumentWhenSessionDocumentMissing ();
	void useFallbackSeedWhenNoDocumentFileExists ();
	void reportConfiguredDocumentParseFailureWithoutFallback ();
	void describeLoadStatusForConfiguredAndFallbackDocuments ();
};
