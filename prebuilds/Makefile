%.framework: %.framework.tar
	tar -xf $< --directory $(dir $<)

%.framework:
	rm -rf $@
	mkdir -p $(dir $@)
	cp -a $< $@
	lipo -create -output $@/$(basename $(notdir $@)) $(addsuffix /$(basename $(notdir $@)),$^)

%.xcframework:
	rm -rf $@
	xcodebuild -create-xcframework -output $@ $(addprefix -framework , $^)

%: %-release.aar
	rm -rf $@
	mkdir -p $@
	unzip -q $< -d $@

all: \
	apple/BareKit.xcframework \
	apple-javascriptcore/BareKit.xcframework \
	darwin/BareKit.xcframework \
	darwin-javascriptcore/BareKit.xcframework \
	ios/BareKit.xcframework \
	ios-javascriptcore/BareKit.xcframework \
	android/bare-kit

apple/BareKit.xcframework: \
	darwin/BareKit.framework \
	ios/BareKit.framework \
	ios-simulator/BareKit.framework

apple-javascriptcore/BareKit.xcframework: \
	darwin-javascriptcore/BareKit.framework \
	ios-javascriptcore/BareKit.framework \
	ios-simulator-javascriptcore/BareKit.framework

darwin/BareKit.xcframework: \
	darwin/BareKit.framework

darwin/BareKit.framework: \
	darwin-arm64/BareKit.framework \
	darwin-x64/BareKit.framework

darwin-javascriptcore/BareKit.xcframework: \
	darwin-javascriptcore/BareKit.framework

darwin-javascriptcore/BareKit.framework: \
	darwin-arm64-javascriptcore/BareKit.framework \
	darwin-x64-javascriptcore/BareKit.framework

ios/BareKit.xcframework: \
	ios/BareKit.framework \
	ios-simulator/BareKit.framework

ios/BareKit.framework: \
	ios-arm64/BareKit.framework

ios-simulator/BareKit.framework: \
	ios-arm64-simulator/BareKit.framework \
	ios-x64-simulator/BareKit.framework

ios-javascriptcore/BareKit.xcframework: \
	ios-javascriptcore/BareKit.framework \
	ios-simulator-javascriptcore/BareKit.framework

ios-javascriptcore/BareKit.framework: \
	ios-arm64-javascriptcore/BareKit.framework

ios-simulator-javascriptcore/BareKit.framework: \
	ios-arm64-simulator-javascriptcore/BareKit.framework \
	ios-x64-simulator-javascriptcore/BareKit.framework
