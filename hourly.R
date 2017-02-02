hourly = read.csv("/Users/gardners/g/power-outage-analysis/hourly.csv", sep=";");
hourly$time <- strptime(hourly$time,format="%Y-%m-%d %H:%M:%OS")

Overall.Cond <- 1:nrow(hourly)
Freq <- hourly$flat_phones
myhist <-list(breaks=Overall.Cond, counts=Freq, density=Freq/diff(Overall.Cond),
              xname="Overall Cond")
class(myhist) <- "histogram"
plot(myhist)


