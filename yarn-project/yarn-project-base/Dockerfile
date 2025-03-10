# FROM 278380418400.dkr.ecr.eu-west-2.amazonaws.com/circuits-x86_64-linux-clang
FROM 278380418400.dkr.ecr.eu-west-2.amazonaws.com/circuits-wasm-linux-clang
FROM 278380418400.dkr.ecr.eu-west-2.amazonaws.com/l1-contracts

FROM node:18-alpine
RUN apk update && apk add --no-cache build-base git python3 curl bash jq
# COPY --from=0 /usr/src/circuits/build/bin /usr/src/circuits/build/bin
COPY --from=1 /usr/src/l1-contracts/out /usr/src/l1-contracts/out
# COPY --from=1 /usr/src/circuits/build/bin /usr/src/circuits/build-wasm/bin

COPY --from=0 /usr/src/circuits/cpp/build-wasm/bin/aztec3-circuits.wasm /usr/src/circuits/cpp/build-wasm/bin/aztec3-circuits.wasm
COPY --from=0 /usr/src/circuits/cpp/build-wasm/bin/aztec3-circuits.wasm /usr/src/circuits/cpp/barretenberg/cpp/build-wasm/bin/primitives.wasm
COPY --from=0 /usr/src/circuits/cpp/barretenberg/cpp/srs_db/download_ignition.sh /usr/src/circuits/cpp/barretenberg/cpp/srs_db/download_ignition.sh
WORKDIR /usr/src/circuits/cpp/barretenberg/cpp/srs_db
RUN ./download_ignition.sh 1

WORKDIR /usr/src/yarn-project

# We only want to copy the package.json's, to ensure we only rebuild this image if project dependencies changed.
COPY acir-simulator/package.json acir-simulator/package.json
COPY archiver/package.json archiver/package.json
COPY aztec-cli/package.json aztec-cli/package.json
COPY aztec-rpc/package.json aztec-rpc/package.json
COPY aztec-node/package.json aztec-node/package.json
COPY aztec.js/package.json aztec.js/package.json
COPY docs/package.json docs/package.json
COPY end-to-end/package.json end-to-end/package.json
COPY ethereum/package.json ethereum/package.json
COPY foundation/package.json foundation/package.json
COPY key-store/package.json key-store/package.json
COPY merkle-tree/package.json merkle-tree/package.json
COPY merkle-tree/package.local.json merkle-tree/package.local.json
COPY noir-contracts/package.json noir-contracts/package.json
COPY noir-compiler/package.json noir-compiler/package.json
COPY l1-artifacts/package.json l1-artifacts/package.json
COPY circuits.js/package.json circuits.js/package.json
COPY p2p/package.json p2p/package.json
COPY p2p-bootstrap/package.json p2p-bootstrap/package.json
COPY prover-client/package.json prover-client/package.json
COPY rollup-provider/package.json rollup-provider/package.json
COPY sequencer-client/package.json sequencer-client/package.json
COPY types/package.json types/package.json
COPY world-state/package.json world-state/package.json
COPY yarn-project-base/package.json yarn-project-base/package.json

# Copy root files
COPY package.json package.*.json tsconfig.json yarn.lock .yarnrc.yml ./
COPY .yarn .yarn

# Although we're attempting to be "zero-install", in practice we still need to build arch specific packages.
RUN yarn --immutable
# If everything's worked properly, we should no longer need access to the network.
RUN echo "enableNetwork: false" >> .yarnrc.yml

# Yarn devs won't provide an extremely simple and useful feature of pruning dev dependencies from the local cache:
# https://github.com/yarnpkg/berry/issues/1789
#
# To work around this, we construct a global cache from the local cache using hard links (requires a hacky rename).
# When we build an upstream docker image, we:
# - Do the build.
# - Erase the local cache with a `yarn cache clean`. Files remain in global cache due to hard link.
# - Do a `yarn workspaces focus --production` to install production dependencies from the global cache, to .yarn/cache
# - A final stage of the build strips away the global cache.
RUN /bin/bash -c '\
  [ -d /root/.yarn/berry/cache ] && exit 0; \
  cd .yarn/cache && \
  mkdir -p /root/.yarn/berry/cache && \
  for F in *; do \
  [[ $F =~ (.*-) ]] && ln $F /root/.yarn/berry/cache/${BASH_REMATCH[1]}8.zip; \
  done'

# We hack around the build context by copying the build manifest to and from the yarn_project root,
# see the comment in .circleci/config.yml for more info.
COPY build_manifest.json ./
RUN cp build_manifest.json ../

# Copy tsconfig to check dependencies
COPY acir-simulator/tsconfig.json acir-simulator/tsconfig.json
COPY archiver/tsconfig.json archiver/tsconfig.json
COPY aztec-cli/tsconfig.json aztec-cli/tsconfig.json
COPY aztec-rpc/tsconfig.json aztec-rpc/tsconfig.json
COPY aztec-node/tsconfig.json aztec-node/tsconfig.json
COPY aztec.js/tsconfig.json aztec.js/tsconfig.json
COPY end-to-end/tsconfig.json end-to-end/tsconfig.json
COPY ethereum/tsconfig.json ethereum/tsconfig.json
COPY foundation/tsconfig.json foundation/tsconfig.json
COPY key-store/tsconfig.json key-store/tsconfig.json
COPY merkle-tree/tsconfig.json merkle-tree/tsconfig.json
COPY noir-contracts/tsconfig.json noir-contracts/tsconfig.json
COPY l1-artifacts/tsconfig.json l1-artifacts/tsconfig.json
COPY circuits.js/tsconfig.json circuits.js/tsconfig.json
COPY p2p/tsconfig.json p2p/tsconfig.json
COPY p2p-bootstrap/tsconfig.json p2p-bootstrap/tsconfig.json
COPY prover-client/tsconfig.json prover-client/tsconfig.json
COPY sequencer-client/tsconfig.json sequencer-client/tsconfig.json
COPY rollup-provider/tsconfig.json rollup-provider/tsconfig.json
COPY types/tsconfig.json types/tsconfig.json
COPY world-state/tsconfig.json world-state/tsconfig.json

# Check that dependencies config are up to date
COPY yarn-project-base/scripts yarn-project-base/scripts
RUN yarn prepare:check


# Generate TS-importable contract artifacts
COPY l1-artifacts/scripts/generate-artifacts.sh l1-artifacts/scripts/generate-artifacts.sh
WORKDIR /usr/src/yarn-project/l1-artifacts
RUN ./scripts/generate-artifacts.sh

WORKDIR /usr/src/yarn-project
