package com.facebook.marianatrench.integrationtests;

class propagateViaArg {

    void testDirectIssue() {
        String tainted = (String) Origin.source();
        StringBuilder mutable = new StringBuilder();
        mutable.append(tainted);
        Origin.sink(mutable);
    }

    void testIndirectIssue() {
        String tainted = (String) Origin.source();
        StringBuilder mutable = new StringBuilder();
        unsafeAppend(mutable, tainted);
        Origin.sink(mutable);
    }

    void testNestedIssue() {
        String tainted = (String) Origin.source();
        StringBuilder mutable = new StringBuilder();
        nestedUnsafeAppend(mutable, tainted);
        Origin.sink(mutable);
    }

    static void testNestedStaticIssue() {
        String tainted = (String) Origin.source();
        StringBuilder mutable = new StringBuilder();
        nestedUnsafeStaticAppend(mutable, tainted);
        Origin.sink(mutable);
    }

    void testIndirectNoIssue(String notTainted) {
        String tainted = (String) Origin.source();
        StringBuilder mutable = new StringBuilder();
        safeAppend(mutable, tainted, notTainted);
        Origin.sink(mutable);
    }

    void testNestedNoIssue(String notTainted) {
        String tainted = (String) Origin.source();
        StringBuilder mutable = new StringBuilder();
        nestedSafeAppend(mutable, tainted, notTainted);
        Origin.sink(mutable);
    }

    void testDirectIssueWithFeature() throws Exception {
        String tainted = (String) Origin.source();
        StringBuilder mutable = new StringBuilder();
        validate(tainted);
        mutable.append(tainted);
        Origin.sink(mutable);
    }

    void testIndirectIssueWithFeature() throws Exception {
        String tainted = (String) Origin.source();
        StringBuilder mutable = new StringBuilder();
        unsafeAppendAndValidate(mutable, tainted);
        Origin.sink(mutable);
    }

    void testNestedIssueWithFeature() throws Exception {
        String tainted = (String) Origin.source();
        StringBuilder mutable = new StringBuilder();
        nestedUnsafeAppendAndValidate(mutable, tainted);
        Origin.sink(mutable);
    }

    void testIndirectIssueWithTransform() throws Exception {
        String tainted = (String) Origin.source();
        StringBuilder mutable = new StringBuilder();
        nestedUnsafeAppendAndTransform(mutable, tainted);
        Origin.sink(mutable);
    }

    void testNestedIssueWithTransform() throws Exception {
        String tainted = (String) Origin.source();
        StringBuilder mutable = new StringBuilder();
        unsafeAppendAndTransform(mutable, tainted);
        Origin.sink(mutable);
    }

    void testNestedIssueWithTransformTryCatch() {
        String tainted = (String) Origin.source();
        StringBuilder mutable = new StringBuilder();
        try {
            nestedUnsafeAppendAndTransform(mutable, tainted);
        } catch (Exception e) {
            return;
        }
        Origin.sink(mutable);
    }

    void testIndirectIssueWithTransformString() {
        // Currently produces two issues "Source" and "Validators-Regex@Source" due to weak updates.
        String tainted = (String) Origin.source();
        indirectTransform(tainted);
        Origin.sink(tainted);
    }

    void nestedUnsafeAppend(StringBuilder mutable, String tainted) {
        unsafeAppend(mutable, tainted);
    }

    void unsafeAppend(StringBuilder mutable, String tainted) {
        mutable.append(tainted);
    }

    static void nestedUnsafeStaticAppend(StringBuilder mutable, String tainted) {
        unsafeStaticAppend(mutable, tainted);
    }

    static void unsafeStaticAppend(StringBuilder mutable, String tainted) {
        mutable.append(tainted);
    }

    void nestedSafeAppend(StringBuilder mutable, String tainted, String notTainted) {
        safeAppend(mutable, tainted, notTainted);
    }

    void safeAppend(StringBuilder s, String tainted, String notTainted) {
        s.append(notTainted);
    }

    void nestedUnsafeAppendAndValidate(StringBuilder mutable, String tainted) {
        unsafeAppendAndValidate(mutable, tainted);
    }

    void unsafeAppendAndValidate(StringBuilder mutable, String tainted) {
        validate(tainted);
        unsafeAppend(mutable, tainted);
    }

    void nestedUnsafeAppendAndTransform(StringBuilder mutable, String tainted) {
        unsafeAppendAndTransform(mutable, tainted);
    }

    void unsafeAppendAndTransform(StringBuilder mutable, String tainted) {
        transform(tainted);
        unsafeAppend(mutable, tainted);
    }

    static void validate(String tainted) {
        if (tainted.contains("not-allowed")) {
            throw new IllegalArgumentException();
        }
    }

    static void indirectTransform(String tainted) {
        transform(tainted);
    }

    static void transform(String tainted) {
    }
}
