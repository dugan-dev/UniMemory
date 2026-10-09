# Security reports

GitHub private vulnerability reporting is enabled. Use [Report a vulnerability](https://github.com/dugan-dev/UniMemory/security/advisories/new) to contact the maintainer privately. Do not post credentials, private application data or unpublished exploit details in public issues or pull requests. If the form is unavailable, request a private contact route in an issue without disclosing the vulnerability.

Include the affected revision, compiler/platform, backend, build options, smallest reproducer and impact. Distinguish a demonstrated memory error from a standards-contract or integration finding. Sanitizers and test success do not establish correctness for arbitrary input.

The version string 0.0.1 does not identify a particular source revision. Include the full commit and dependency versions. Source is distributed from main without tags or Releases; no long-term support, backport schedule or response-time guarantee is promised. Coordinate publication of technical details and fixes through the private report.

Memory instances, owners and containers must obey the [lifetime contract](docs/compatibility.md). Destroy users before reset/rewind/destruction, pair allocation with its original size/alignment and context, and synchronize reclamation.
