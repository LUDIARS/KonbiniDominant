"""Package the Phase 1-4 Windows build without launching it."""
import sys
sys.dont_write_bytecode = True
from package_game import main

if __name__ == "__main__":
    main(4, "Phase1-4")
