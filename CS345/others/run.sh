for dir in grp/*/
do
    cd $dir
    javac *.java  -Xlint:unchecked 
    # java ArraySortGrade > 1array_result.txt
    java SortGridGrade_1 > 5grid_result.txt
    cd ../../
done