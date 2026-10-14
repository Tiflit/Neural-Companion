# Source review and attribution rules

Established owner direction: inspect third-party licences before using repositories to inform our code, and credit others' work whenever used. These rules cover human and AI-assisted contributions. They supplement the component policy without installing the unfinished multi-agent workflow template.

## Before implementation study

1. Identify the source, author/project, exact revision, and intended use.
2. Read its governing licence, relevant notices, file headers, and dependency provenance. Record findings in [the licence register](LICENSE-REVIEW.md).
3. Classify whether the source is a conceptual reference, copied/adapted material, linked dependency, or runtime integration.
4. Resolve the permissions and compatibility for that use before transferring implementation detail into our code. Missing or ambiguous rights block incorporation.
5. If code was already viewed, record that fact; do not claim independent or clean-room implementation merely because the final text was typed anew.

Checking a repository's licence is not the same as approving all its files. Restricted or unlicensed code must not be supplied to a code-generation agent as implementation material for this project.

## Distinguish the kinds of use

| Use | Project requirement |
|---|---|
| Conceptual inspiration | Cite the source and describe the general idea in original wording. Work from public requirements and independently develop the implementation. Record material influence even when attribution is voluntary. |
| Source-derived adaptation | Preserve the original licence/copyright, identify exact source files and revision, describe changes, and satisfy all applicable derivative-work obligations. |
| Verbatim snippet, shader, configuration, test, or documentation | Treat as third-party material; review rights and preserve required notices before incorporation. |
| Linked/vendored dependency or binary | Review exact artifact and transitive terms; include required licences/notices and source obligations in all relevant distributions. |
| Proprietary provider | Apply component policy; supported integration with established rights only. No extraction, conversion, patching, or circumvention. |

General ideas and protected expression are different, but the boundary can be fact-specific. This project takes a conservative approach to implementation-level borrowing. A credit line is not permission.

Renaming variables, translating languages, restructuring, paraphrasing code, or asking an AI to rewrite it does not automatically remove source-derived obligations. Do not remove provenance to make an adaptation look original.

## Default treatment

- MIT/BSD/zlib/Apache sources: candidates, not automatic approvals. Preserve their exact applicable terms and notices. Apache adaptations also need applicable NOTICE handling and change notices; BSD variants may differ.
- Copyleft sources: block incorporation until a concrete licensing/integration decision resolves source, distribution, and compatibility requirements. Do not assume a separate DLL/process is a universal exemption.
- Noncommercial/no-derivatives/restrictive terms: no code adaptation, linking, packaging, or integration without permission sufficient for the proposed use.
- No licence or uncertain rights: no implementation reuse pending clarification. Public visibility, GitHub forking, download availability, or lack of enforcement is insufficient.
- Mixed licence repositories: review the particular paths and their dependency chain.
- Model reimplementations dependent on extraction/recovery: excluded as inference backends under the project's scope, even if their own source licence is permissive.

These are project risk controls, not claims that every listed activity is legally prohibited in every jurisdiction.

## Adoption record required in the same change

For every actual reuse, add a record containing:

- Upstream project and author/rights holders as identified by their notices.
- Immutable commit/source URLs and exact source paths.
- Our destination paths and whether copied, adapted, linked, packaged, or conceptually influenced.
- Governing licence paths, copyright and NOTICE requirements, dependency chain.
- Changes made, including material AI-assisted transformations.
- Permission/compatibility basis for the specific use and unresolved limitations.
- Locations of retained licence texts, source headers, and release notices.
- Required corresponding-source mechanism where applicable.

Record the adoption in [THIRD-PARTY-NOTICES.md](../THIRD-PARTY-NOTICES.md); keep full required notices with shipped code/artifacts. A URL alone does not substitute for required licence text. Material conceptual influence is credited separately from incorporated components so users can see the difference.

Use SPDX identifiers only when supported by the actual licence; do not invent copyright ownership or replace upstream headers with our MIT header. Preserve relevant transitive attribution. Do not copy every upstream contributor name indiscriminately or claim endorsement.

## Submission and release checks

Each implementation change should disclose either its sources and adoption records, or that no external material was incorporated. Review imported examples, generated code, shaders, images, tests, and configuration as well as conventional libraries.

Before distribution:

- Reconcile the actual shipped files and dependency inventory with adoption records.
- Include required licence/NOTICE/copyright texts and modification statements.
- Satisfy source-delivery and redistribution requirements for the actual integration.
- Check archives, generated assets, caches, and repository history for accidentally included proprietary material.
- Remove unsupported compatibility or endorsement claims.
- Record unresolved rights and withhold the affected component until resolved.

No automated enforcement is implemented yet. These are documented review gates; the future workflow template can add checks without implying that a scanner certifies compliance.

## Permission questions

Ask rights holders only when needed, with exact component/use/distribution details. Preserve written permission in an appropriately scoped record; sanitize personal correspondence before public publication. No outreach is authorized by this document alone.

Patent, trademark, contract, game-EULA, anti-circumvention, and jurisdiction-specific questions can remain even after copyright licence review. Obtain qualified advice for unresolved material issues; do not represent this process as legal immunity.

## References

- [GitHub guidance on repository licensing](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository)
- [Apache License 2.0, including redistribution requirements](https://www.apache.org/licenses/LICENSE-2.0)
- The exact upstream licence and notices linked by [the review register](LICENSE-REVIEW.md).
