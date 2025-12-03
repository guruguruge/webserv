fmt:
	@echo "Formatting code..."
	@find . -type f \( -name "*.cpp" -o -name "*.hpp" \) -not -path "./.*" -exec clang-format -i {} +
	@echo "Done."

.PHONY: fmt
