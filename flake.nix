{
  description = "Testing Sokol libs as a bases for graphic engine";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      systems = [ "aarch64-linux" "aarch64-darwin" "x86_64-darwin" "x86_64-linux" ];
      eachSystem = systems: f: builtins.listToAttrs (builtins.map (system: { name = system; value = f system;}) systems);
    in {
      devShells = eachSystem systems (system:
        let
          pkgs = import nixpkgs { inherit system; };
        in {
          default = import ./shell.nix { inherit pkgs; };
        });
    };
}
