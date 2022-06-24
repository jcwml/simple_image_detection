# simple_image_detection
A simple image detection algorithm applied to CS:GO.

I thought it might be worth a try, sadly it failed, although has a potential to work on less noisy datasets.

The concept is to take a dataset of images and average every individual pixel on each image in the dataset down to one averaged greyscale pixel map, that includes the min and max averages too.

From this the (max-min) deviance is used to ignore pixels in the final map for optimisation, and then a 0-1 score is produced based on how close each remaining pixel was to it's expected average pixelmap value.

This 0-1 score is our final probability of how similar the image being fed into the model was to the images in the dataset.
