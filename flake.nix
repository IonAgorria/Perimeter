{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    systems.url = "github:nix-systems/default";
  };

  outputs = { self, nixpkgs, systems, ... }: (
    let
      eachSystem = nixpkgs.lib.genAttrs (import systems);
      pkgsFor = nixpkgs.legacyPackages;
    in {
      packages = eachSystem (system: {
        default = pkgsFor.${system}.callPackage ./perimeter.nix {
          src = self;
        };
        debug = pkgsFor.${system}.callPackage ./perimeter.nix {
          src = self;
          flag_debug = true;
        };
      });

      devShells = eachSystem (system: (
        let
          package = self.packages.${system}.default;
          mkDevShell = extras: pkgsFor.${system}.mkShell {
            buildInputs = package.buildInputs ++ package.nativeBuildInputs ++ extras;
          };
        in {
          default = mkDevShell [];
          clang = mkDevShell [ pkgsFor.${system}.clang ];
        }
      ));
    }
  );
}
