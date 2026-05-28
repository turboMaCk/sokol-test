{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell rec {
  nativeBuildInputs = with pkgs; [
    pkg-config gcc clang clang-tools bear
  ];
  buildInputs = with pkgs; [
    libGL
  ] ++ (with pkgs.xorg; [
    libX11 libXi libXcursor libXrandr libXext libXext libXfixes
  ]);

  shellHook = ''
      export C_INCLUDE_PATH="${pkgs.glibc.dev}/include:$C_INCLUDE_PATH"
    '';
}
