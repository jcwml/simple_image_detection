# simple_image_detection
A simple grey-scale pixel map image detector.

The plan was to generate a greyscale pixel map from the RGB sample images and then discard the pixels with a large (max-min) deviance that was too large so that less pixels needed to be scanned and only the areas of low deviance could be focused on.

Then, I would test that pixel map of low deviance pixels against the original image samples and their nontarget samples increasing the tolerance on each epoch _(the tolerance being a range value from the avg pixel colour that is accepted)_ until all 255 tolerance values are tested and calculate which tolerance value had the best detection on targets and least error on nontargets.

This would leave me with a concise pixel map with good detection results.

... only as you can see the lowest deviance per grayscale pixel in the target dataset is 229 which means this won't work on this particular dataset.

But it's a cool idea that might work on something else, like a runescape bot.

The final pixel map could generate C source code calling a is_pixel_in_tol(x, y, tol) function for each individual pixel that passed the minimum deviance test.

very efficient, no loops, very compact, ready to deploy.
