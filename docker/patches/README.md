# Patches applied to the pinned upstream trees

`src/dc/decomp/` shadows a decomp *header* when the port needs to see it
differently. That covers everything the port can express from outside the
tree. What it cannot express is a change to a **struct layout**: a shim
cannot re-declare a type the real header also declares, and copying a
590-line header to change six lines of it is the fork this project has said
it will not keep.

So the remaining edits go here, as patches applied with `git apply` to the
checkout in the image's `src` stage (`docker/Dockerfile`), never as a fork.
The rules:

- One patch, one reason. The filename says which.
- A patch must leave the decomp's own build byte-identical. In practice that
  means guarding the new code on something IDO does not define.
- Every patch should be something upstream would take. If it would not, it
  probably belongs in `src/dc/decomp/` or in the port's own code instead.
- Each patch's header says why it exists.

The patches apply against `DECOMP_REV` in `docker/pins.env`. Bumping that pin
means re-checking them; `docker/apply-patches.sh` fails the image build loudly
rather than skipping one that no longer applies.
