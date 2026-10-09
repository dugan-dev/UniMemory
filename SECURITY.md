# Security reports

Avoid posting credentials, private application data or unpublished exploit details in public issues. Use a private channel agreed with the repository maintainer; when GitHub's private vulnerability reporting is enabled, use the repository Security tab's reporting form.

Include the affected revision, compiler/platform, backend, build options, smallest reproducer and impact. Distinguish a demonstrated memory error from a standards-contract or integration finding. Sanitizers and test success do not establish correctness for arbitrary input.

Memory instances, owners and containers must obey the [lifetime contract](docs/compatibility.md). Destroy users before reset/rewind/destruction, pair allocation with its original size/alignment and context, and synchronize reclamation. Never use the library's test fixtures as production credentials or configuration.
